# Flasheo del firmware open source — RT-1000Pro

## Antes de flashear nada

1. **Guarda el firmware oficial V1.16** (`rt1000-re/firmware.hex`,
   243.772 bytes). Es tu única vía de regreso. Sin él, un brick
   es permanente.
2. **Nunca flashees la app completa primero.** El hardware del
   1000Pro NO está confirmado (ver `RE-ANALYSIS-2026-11-17.md`
   y la sección de hardware abajo). El primer flash debe ser
   un build de prueba de hardware.
3. El bootloader de fábrica (`0x08000000–0x080027FF`) nunca se
   toca: todos los `.hex` de este proyecto empiezan en
   `0x08002800`.

## Paso 1 — Test de display (TEST=3)

```sh
cd rt1000-open
python3 updater/rt1000_update.py firmware/hwtest/rt1000-open-hwtest-3.hex --verify-only
python3 updater/rt1000_update.py firmware/hwtest/rt1000-open-hwtest-3.hex --port /dev/ttyUSB0
```

Si la pantalla muestra 8 barras de color (rojo, verde, azul,
blanco, amarillo, magenta, cian, negro), el display ST7789V y
el bus 8080 (datos PD8–PD15, WR PD0, CS PD1, RST PD2, DC PD3,
backlight PC6+PB3) coinciden con el candidato. Si no muestra
nada o muestra basura: **para aquí**, no flashees nada más, y
anota qué se ve.

## Paso 2 — Test del transceptor (TEST=4)

```sh
python3 updater/rt1000_update.py firmware/hwtest/rt1000-open-hwtest-4.hex --port /dev/ttyUSB0
```

Conecta la UART (115200 8N1) y lee la salida. El test lee
REG_0C/REG_30 del BK4829 por SPI bit-bang (CS0 PE8, CS1 PE15,
SCK PE10, SDA PE11). Si responde valores distintos de
`0x0000`/`0xFFFF`, hay un BK4829 (o BK4819) en esos pines.

## Paso 3 — Solo si 1 y 2 pasan: la app

```sh
python3 updater/rt1000_update.py firmware/build/rt1000-open.hex --port /dev/ttyUSB0
```

La app arranca con splash, VFO A 146.520 MHz, VFO B 435 MHz,
teclado y GPS. TX solo en bandas de aficionado; airband TX
sigue deshabilitado (ver `PROTOCOL.md` §6).

## Recuperación

Si algo sale mal, reflashea el oficial:

```sh
python3 updater/rt1000_update.py ../rt1000-re/firmware.hex --port /dev/ttyUSB0
```

El updater oficial (Ido_Update) también sirve para volver.

## Hardware: lo que se sabe y lo que no

- **Descartado: AT32F403A** (MCU del RT-950 Pro). Sus bases
  GPIO son `0x40010800` (GPIOA) etc.; el V1.16 del 1000Pro usa
  `0x40020000` (GPIOA), `0x40023800` (RCC), `0x40004400`
  (USART2). Eso es mapa STM32F4 / GD32F4 / APM32F4. Por eso el
  firmware del RT-950 Pro no se puede portar tal cual: ni las
  bases GPIO, ni la base de app (`0x08002800` vs `0x08003000`),
  ni el flash (256 KiB vs 1 MB), ni el update (HEX plano vs
  `.BTF` cifrado) coinciden.
- **Candidato, sin verificar en el 1000Pro:** MCU familia
  STM32F4 (GD32F4/APM32F4), transceptor BK4829 (x2), receptor
  SI4732, display ST7789V 240x320, flash SPI 25Q. Pines del
  pinmap del RT-950 Pro (Hertzz58, verificado en ese radio).
- **Falta:** marcado del chip + SWD IDCODE, y los resultados
  de los tests 3 y 4 en el radio real.
