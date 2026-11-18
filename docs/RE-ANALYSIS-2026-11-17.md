# Re-análisis estático — firmware V1.16 (2026-11-17)

Continuación de `PROTOCOL.md`. Sin hardware, solo con el binario
extraído (`rt1000-re/firmware.bin`, 243.772 bytes, base `0x08002800`).
Reproducible con `docs/analyze_v116.py` (requiere `capstone`).
Nada de esto identifica el MCU con certeza: lo que hace es
estrechar las hipótesis y dejar constancia de lo que NO cuadra.

## 1. Lo confirmado de nuevo

- SP inicial `0x2000AEC8`, Reset `0x08002AC1` (Thumb, código en
`0x08002AC0`): trampolín `LDR r0,[PC,#0x24]; BLX r0` → init
`0x08022FA9`, `LDR r0,[PC,#0x24]; BX r0` → entry `0x080029E1`.
Los literales están en `0x08002AE8` / `0x08002AEC` — verificado.
- Entry `0x080029E1` es CRT init (copia `.data`, limpia `.bss`,
procesa tabla de init), no `main()` directamente.
- Default handler en `0x08002AD8`: `b.` (bucle infinito).
La mayoría de los 64 vectores apuntan ahí.

## 2. Vector table: solo 6 IRQs reales

| Vector | IRQ | Handler | En STM32F4 sería | En GD32F4 sería |
|---|---|---|---|---|
| 27 | 11 | `0x08008351` | DMA1_Stream0 | DMA0_Channel0 |
| 34 | 18 | `0x08002D1D` | ADC | ADC0_1 |
| 41 | 25 | `0x080231A1` | TIM1_UP_TIM10 | TIMER0_UP_TIMER9 |
| 44 | 28 | `0x080233D1` | TIM2 | TIMER1 |
| 45 | 29 | `0x0802348D` | TIM3 | TIMER2 |
| 54 | 38 | `0x08026A2D` | USART1 | USART0 |

Más las excepciones: MemManage `0x080056ED`, BusFault `0x08026C0D`,
UsageFault `0x0802066D`, SVCall `0x080084AB`, DebugMon `0x0801DFE5`.

El handler DMA1_Stream0 (`0x08008350`) es corto: llama a
`0x080276E8` / `0x080276C8` y pone un flag a 1. El de ADC y los
de TIM son handlers reales, no stubs.

### La anomalía del USART (IRQ 38)

El handler de IRQ 38 (`0x08026A2C`) **no** toca `0x40011000`
(USART1 en STM32 / USART0 en GD32). Sus literales son
`0x40004400` (USART2 en STM32 / USART1 en GD32) y `0x40011400`
(USART6 en STM32 / USART5 en GD32). Es decir:

- Si el mapa es STM32F4, el handler de la IRQ de USART1 está
atendiendo USART2 y USART6 — o es un handler compartido que
sondea varios USARTs, o la tabla no es la de STM32F4.
- Si el mapa es GD32F4, IRQ 38 es USART0 (`0x40011000`), y
tampoco cuadra: el handler toca USART1/USART5 de GD32.

Conclusión: **la tabla de IRQs por sí sola no distingue
STM32F4 de GD32F4** (los números 11/18/25/28/29 coinciden en
ambos), y el handler USART sugiere que el firmware sondea
varios puertos en una sola IRQ, o que el vendor reordenó
vectores. No afirmar MCU por la tabla.

## 3. SystemInit en `0x08022FA8` — decodificado

Desensamblado con Capstone (Thumb, direcciones corregidas -1):

```
CPACR 0xE000ED88 |= 0x00F00000; habilita FPU CP10/CP11 -> Cortex-M4F
delay(1)
RCC 0x40023800:
RCC_CR: BIC #1; ADD #1; HSION=1, espera HSIRDY (bit 1)
RCC_CFGR (+0x08): BIC #3; SW=HSI, espera SWS (bits 2-3) == 0
RCC_CR: &= 0xFEF2FFFF; apaga PLL/HSE/CSS (bits 16,18,19,24...)
RCC_CFGR (+0x08) = 0x40000000
RCC_PLLCFGR (+0x04) = 0x00033002
RCC (+0xA0) = 0x000F0000; offset sin equivalente STM32F4
RCC_CIR (+0x0C) = 0x009F0000; limpia flags de interrupción de reloj
delay(0)
SCB_VTOR (0xE000ED08) = 0x08000000; VTOR apunta al inicio de flash,
; no a la app en 0x08002800
```

### El problema del PLL

`PLLCFGR = 0x00033002` decodifica, en layout STM32F4/GD32F4, como:

- PLLM = 2, PLLN = 192, PLLP = /8, PLLSRC = 0 (HSI), PLLQ = 0

Con HSI de 16 MHz: VCOin = 8 MHz, VCO = 1536 MHz, SYSCLK = 192 MHz.
Eso está **fuera de rango** para STM32F4 (VCO 192–432 MHz,
VCOin 1–2 MHz, SYSCLK máx. 168 MHz),
y el `PLLQ = 0` tampoco es válido (USB necesita /2–/15).
El SystemInit de GD32F4, además, resetea el PLL a
`0x24003010`, no a `0x00033002`.

Interpretaciones posibles, en orden de prudencia:

1. **El PLL queda apagado en SystemInit** (la máscara lo apaga)
y la configuración real de reloj se hace después, en otra
función que aún no se ha localizado. El valor `0x00033002`
sería entonces un placeholder, no la frecuencia final.
2. **El MCU no es STM32F4 ni GD32F4**, sino un clon con otro
layout de PLL (o con VCO/SYSCLK hasta 192 MHz). El offset
`+0xA0` sin equivalente STM32F4 apunta en esa dirección.
3. La decodificación de `PLLCFGR` es correcta pero el HSI no
es de 16 MHz en este chip (aun así, ningún HSI razonable
deja el VCO en 192–432 MHz con PLLM=2/PLLN=192).

**No usar 192 MHz como dato.** El reloj real hay que medirlo
por hardware (o encontrar la función de reloj que enciende
el PLL después de SystemInit).

## 4. GPS: sin cadenas `$GP`, pero con parser en la IRQ

No hay ninguna cadena `$GP`, `GPRMC` ni `GPGGA` en el binario.
Sin embargo, el handler USART (`0x08026A2C` en adelante)
compara bytes recibidos con:

- `0x24` = `$` (inicio de sentencia NMEA)
- `0x2A` = `*` (antes del checksum NMEA)
- `0x52` = `R`, `0x4D` = `M` (de `RMC`)

Es decir, el GPS se parsea por bytes en la interrupción,
buscando `$...RMC*xx`, sin guardar la cadena literal
`$GPRMC` en flash. Eso explica la ausencia de cadenas NMEA
y confirma que el puerto GPS es uno de los USARTs que toca
ese handler (`0x40004400` / `0x40011400`).

El módulo `firmware/src/nmea.c` implementa ese parser
(`$GPRMC` / `$GPGGA`, checksum XOR) para probarlo en host.

## 5. SPI flash externa: lista `25Q` confirmada

En `0x0800D278`: `25Q80 1MB`, `25Q16 2MB`, `25Q32 4MB`,
`25Q64 8MB`, `25Q128 16MB`, `25Q256 32MB`, `25Q512 64MB`,
`Unknown`. Es la detección por JEDEC ID de la flash externa
(W25Q / GD25Q / compatible). El tamaño real del equipo hay
que leerlo por SPI (comando `0x9F`), no asumirlo.

## 6. Periféricos más referenciados (literales u32)

GPIOA `0x40020000` (44), GPIOB `0x40020428` (18, registro
ODR/BSRR de GPIOB), `0x40021428` (GPIOF), USART2 `0x40004400`
(8), GPIOC `0x40020800` (7), TIM3 `0x40000400` (7),
USART6 `0x40011400` (6), DAC `0x40007410` (5), TIM1
`0x40010000` (5), ADC1 `0x40012000` (4), SPI2 `0x40003800`
(4), SPI3 `0x40003C00` (2), I2C2 `0x40005800` (3), DMA1
`0x40026000` (3). Sin referencias a UID (`0x1FFF7A10` /
`0x1FFFF7E8`) ni a DBGMCU (`0xE0042000`): el firmware no
lee su propio ID, así que el IDCODE solo se puede leer
por SWD.

## 7. Lo que sigue sin saberse (hardware, obligatorio)

- MCU exacto: leer marcado del chip + DBGMCU IDCODE por SWD.
Candidatos: STM32F4xx, GD32F4xx, o clon pin-compatible.
La FPU (M4F) y el mapa `0x40023800` estrechan a esa familia.
- Transceptor: ninguna cadena lo nombra. Identificar por
marcado / analizador lógico en SPI2/SPI3/I2C2.
- Display, teclado, pines: sin mapear. GPIOA es el puerto
más usado (44 refs), seguido de GPIOB/GPIOF.
- Reloj real: medir SYSCLK / HSE, localizar la función que
enciende el PLL después de SystemInit.
- Bootloader de fábrica (`0x08000000–0x080027FF`): no está
en el HEX. No tocarlo nunca.

## 8. Addendum 2026-11-17 (noche): el RT-950 Pro NO es el 1000Pro

Fuentes: firmware open source del RT-950 Pro (Hertzz58,
`Radtel-RT950-Pro-Firmware`, y jaymart1983, `research/FINDINGS.md`),
clonado y leído en `/tmp/rt950`. Su hardware confirmado en ese
radio: MCU **Artery AT32F403A** (Cortex-M4F @ 120 MHz, 1 MB flash,
96 KB SRAM), dos BK4829 (CS PE8/PE15, SCK PE10, SDA PE11),
SI4732 por I2C bit-bang (PB6/PB7, addr 0x11), display ST7789V
240x320 por bus 8080 (datos PD8–PD15, WR PD0, CS PD1, RST PD2,
DC PD3, backlight PC6 + PB3), teclado matriz PC0–PC3 / PD4–PD7,
UART4 CPS (PC10/PC11, 115200), USART3 GPS (PB10/PB11, 9600),
USART1 Bluetooth (PA9/PA10). App en `0x08003000`, SP
`0x20017BB0`, update en `.BTF` cifrado.

**El 1000Pro es otra plataforma.** Su V1.16: app en
`0x08002800`, imagen 256 KiB, SP `0x2000AEC8`, update HEX plano
(Ido_Update). Y lo decisivo: el AT32F403A usa GPIOA
`0x40010800` y RCC en `0x40021000`; el V1.16 usa GPIOA
`0x40020000`, RCC `0x40023800`, USART2 `0x40004400` (el USART
más referenciado; en el 950, USART2 tiene CERO referencias).
Ese es el mapa STM32F4 / GD32F4 / APM32F4.

Consecuencias:
- **AT32F403A descartado** para el 1000Pro.
- **No se puede portar el firmware del 950 tal cual**: ni las
  bases GPIO, ni la base de app, ni el flash, ni el formato de
  update coinciden. Portarlo = brick.
- Lo que sí se reutiliza del 950: el mapa de registros del
  BK4829 (REG_07/08 freq, REG_30 modos, REG_36 TX power,
  REG_0C RSSI), el protocolo 8080 del ST7789V, la matriz de
  teclado corregida y el layout de canales de 32 bytes —
  adaptados al mapa del 1000Pro, no copiados como binario.
- El FCC tampoco ayuda: Radtel (grantee 2AZSA) tiene 20 FCC
  IDs y ninguno es el RT-1000Pro. El PDF `2ANMU-Y1000PRO` que
  apareció en la búsqueda es un teléfono MediaTek (MT6835 /
  MT6177, 2019), no el radio — descartado tras revisar las
  fotos de los chips.
- El MCU exacto del 1000Pro sigue sin saberse: candidatos
  STM32F401/405/407, GD32F403/405, APM32F407 y clones. Solo
  lo confirman el marcado del chip + SWD IDCODE, o los tests
  de hardware (ver `FLASHING.md`).
