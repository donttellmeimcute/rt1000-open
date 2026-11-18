# Firmware open source — RT-1000Pro (flasheable tras tests de hardware)

Este directorio es el firmware alternativo open source para el
Radtel RT-1000Pro / UV100: app completa con UI, menu, GPS y APRS.
Flasheable con el updater Python, PERO solo despues de que los
tests de hardware (TEST=3 LCD y TEST=4 BK4829) confirmen el
display y el transceptor en el radio real.

## Qué sí hay (actualizado 2026-11-17, noche)

- Mapa de memoria verificado por ingeniería inversa:
  - Flash 256 KiB en `0x08000000–0x0803FFFF`
  - Bootloader de fábrica: `0x08000000–0x080027FF` (10 KiB, no tocar)
  - Aplicación: `0x08002800–0x0803FFFF` (246 KiB)
  - SP inicial del firmware V1.16: `0x2000AEC8`, reset `0x08002AC1`,
    entry (registro `:05` del HEX): `0x080029E1`
- Re-análisis estático en `../docs/RE-ANALYSIS-2026-11-17.md`
  (reproducible con `../docs/analyze_v116.py`): CPU Cortex-M4F
  (FPU habilitada en SystemInit), solo 6 IRQs reales, GPS parseado
  byte a byte en la IRQ USART (`$`, `*`, `R`, `M` de RMC, sin
  cadenas `$GPRMC` en flash), lista de flash externa `25Q80`–
  `25Q512`, y una anomalía: el PLL de SystemInit
  (`PLLCFGR=0x00033002`, PLLM=2/PLLN=192/PLLP=/8/HSI) daría
  VCO 1536 MHz / SYS 192 MHz, fuera de rango STM32F4 — el PLL
  queda apagado ahí y el reloj real se configura después (o el
  MCU es un clon con otro layout). No usar 192 MHz como dato.
- Módulos nuevos en `src/`, probados en host (`make test`, 21/21):
  - `hal.h` / `hal_mock.c` — HAL abstracta (UART 115200, SPI,
    flash de app con guarda del bootloader) + mock en memoria.
  - `clock.c` — decodificador del PLL y `rt1000_system_init_documented()`,
    que reproduce los writes observados (solo ARM, solo tras
    confirmar el MCU).
  - `bootloader_rx.c` — receptor completo del protocolo lado radio:
    4× CONNECT → UPDATE → 246 paquetes, ACK/NAK, escritura a flash,
    rechazo de paquete corrupto. Probado contra paquetes generados
    con el mismo checksum que el updater Python.
  - `nmea.c` — parser `$GPRMC` / `$GPGGA` con checksum XOR;
    extrae satélites (GGA), velocidad y rumbo (RMC).
  - `aprs.c` — frame AX.25 UI + position packet (callsign por
    parámetro, no hardcodeado; AFSK/modulación es de hardware),
    con path de digipeaters (WIDE1-1,WIDE2-1...) y FCS X.25.
  - `vfo.c` — modelo VFO, rangos RX y TX permitido (airband TX: no),
    cálculo TX con shift/offset/talkaround/reverse, canales NOAA
    WX1-WX7 y avance de scan con wrap por banda.
  - `tones.c` — CTCSS 48 tonos, DCS 104 códigos, DTMF
    (frecuencias por tecla) y codificación de tono en settings.
  - `storage.c` — canales BCD 32B, flash SPI 25Q (JEDEC, último
    sector para nuestros datos) y persistencia de ajustes+canales
    con magic y checksum XOR.
  - `ui.c` / `app.c` — UI completa (ST7789V 240x320): splash con
    callsign, pantalla principal con VFO A grande (3x) + VFO B,
    S-meter del BK4829, barra de estado (callsign, RX/TX, GPS,
    MON, SCN, MDM), menu real de 40 items con scroll (VFO, paso,
    potencia, modo, squelch, GPS, APRS, luz, tema, callsign,
    nombre, scan, NOAA, radio FM, APRS SSID/path/detalle, DTMF,
    MDC ID, VOX, ancho de banda, tone burst, CTCSS RX/TX, shift,
    offset, talkaround, reverse, monitor, modo modem, formato GPS, TNC, zona,
    prioridad TX, multi standby, APO (auto off: OFF/15/30/60/120 min,
    aviso "Power Off!" 10 s antes, cualquier tecla lo cancela), alarma, backup, reset, canales,
    info), pantalla GPS (sats, velocidad, rumbo, 3 formatos),
    canales en 4 zonas x 4 slots (guardar/recuperar con ABC/OK,
    persistentes en flash), radio FM (SI4732, 87.5-108 MHz),
    detalle APRS (FCS Check), editor de texto (callsign, nombre,
    DTMF, MDC), alarma de emergencia y entrada de frecuencia por
    teclado (`*` es el punto decimal). Tema de color en el menu
    (item "Tema"): KAWAII rosa claro por defecto
    (`ui_set_theme(1)`), ORIGINAL negro/verde con L/R.
    El GPS ya se alimenta del parser NMEA con los bytes de UART.
    `make emu` renderiza todas las pantallas en `emu-out/`
    (incluye `09-principal-original.png` para comparar temas).
    Inventario completo de funciones en `../docs/PORTED.md`.
- `linker.ld` y `Makefile` para `arm-none-eabi-gcc`, Cortex-M4 Thumb.
  `make test` usa solo gcc de host y pasa (TODO OK); `make arm`
  compila con gcc-arm-none-eabi 15:13.2.rel1-2 y genera
  `build/rt1000-open.hex` (56.856 bytes, SP 0x2000AEC8,
  Reset 0x08002849), verificado con el updater
  (--verify-only / --dry-run, 246 paquetes OK).
  Builds de prueba de hardware en `hwtest/` (TEST=3 LCD,
  TEST=4 BK4829), tambien verificados. Ver `../docs/FLASHING.md`.

## Qué NO hay (y por qué no se puede inventar)

- **MCU exacto sin confirmar.** El mapa de periféricos es compatible
  con la familia STM32F4 / GD32F4 (GPIO `0x40020000`, RCC `0x40023800`,
  USART1/2/3/6, SPI2/3, I2C), pero eso no distingue STM32 de GD32,
  AT32 o APM32. Hay que leer el marcado del chip y el IDCODE
  (DBGMCU) con SWD antes de elegir el target.
- **Chip de radio sin confirmar.** No hay ninguna cadena en el
  firmware que nombre el transceptor (BK4819, AT1846S, etc.).
  Hay que identificarlo por marcado / analizador lógico en SPI/I2C.
  Los registros BK4829 de VOX, ancho de banda, tone burst y tonos
  no están documentados en la referencia del RT-950: las
  selecciones se guardan, pero su aplicación al chip está pendiente.
- **Display, teclado, GPS, APRS, audio, SPI flash externa.**
  Los drivers están escritos con los candidatos del RT-950 Pro
  adaptados al mapa del 1000Pro y probados en host; sus pines
  reales no están mapeados todavía. El camino de audio (AFSK
  para APRS, DTMF/MDC por DAC) no está confirmado.
- **Bootloader de fábrica.** No está en el HEX extraído (empieza
  en `0x08002800`). No lo borres nunca: sin él no hay forma de
  recuperar el radio por serie.

## Regla de seguridad

No generes un `.hex` desde este esqueleto y lo flashees. El código
actual es un esqueleto que **no inicializa el radio** y que, si se
flasheara, dejaría el equipo sin RX/TX hasta recuperar el firmware
oficial por serie (si el bootloader sigue intacto).

El camino correcto es: identificar MCU y radio por hardware →
mapear pines con multímetro / lógica → portar driver por driver →
probar en RAM / con SWD → solo entonces, flashear una build
completa y verificada.

## Compilar y probar

```sh
# Pruebas en host (sin toolchain ARM, lo verificable hoy)
make test   # TODO OK: checksum, PLL, NMEA, APRS+FCS, VFO, TX freq, NOAA, scan, tonos, flash persist, bootloader RX

# Emulacion del display: renderiza las pantallas reales a PNG
make emu    # -> emu-out/*.png (splash, principal, menu, GPS, canales, FM, APRS, editor, alarma...)

# Build ARM (requiere toolchain; instalacion pendiente de aprobacion)
arm-none-eabi-gcc --version   # toolchain ARM Cortex-M
make arm
```

Teclas en la app: Up/Down mueve el VFO activo (paso configurable),
Left/Right cambia el paso, V/M cambia de VFO, OK abre el menu,
`#` abre la pantalla GPS, ABC abre canales, digitos/`*` entran
una frecuencia directamente, Back sale de la pantalla actual.

`make arm` genera `build/rt1000-open.elf` y `.hex`: una app
completa (splash, VFO A/B, teclado, GPS, APRS, RF). Es
flasheable con el updater Python, PERO solo despues de que
los tests de hardware (TEST=3 y TEST=4) confirmen el display
y el transceptor en el radio real. Sin eso, no flashear la app.
