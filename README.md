# RT-1000Pro Open — Radtel RT-1000Pro / UV100, open source

Ingeniería inversa del actualizador oficial
`Radtel_RT-1000Pro_Update_260611` y reimplementación abierta
del protocolo de update, más el esqueleto de un firmware
alternativo.

> ⚠️ **Lee esto primero:** flashear puede dejar el radio
> inservible si se interrumpe o si el HEX no es de este modelo.
> El updater open source está probado contra el protocolo
> (mock + dry-run), **no contra un radio real**. El firmware
> open source es solo un esqueleto y **no se debe flashear**.

## Qué se encontró

- El `.exe` oficial es un updater .NET 4.0 (`Ido_Update`),
  no el firmware. El firmware va embebido como una cadena
  Intel HEX de 15.242 registros.
- Firmware **V1.16, 2026-06-11, 243.772 bytes**,
  en flash `0x08002800–0x0803E03B`, plataforma **UV100**,
  CPU ARM Cortex-M Thumb-2, mapa de periféricos compatible
  STM32F4 / GD32F4 (sin confirmar el chip exacto).
- Protocolo serie **115200 8N1, DTR on**: 4 × CONNECT,
  1 × UPDATE, 246 paquetes de 1028 bytes (`'W'` + offset
  + 1024 datos + checksum `sum + 'R' + 'T' + 1`).
- Detalle completo, con bytes, checksums y máquina de estados:
  [`docs/PROTOCOL.md`](docs/PROTOCOL.md)

## Estructura

```
updater/       rt1000_update.py — updater en Python (pyserial)
               --verify-only / --dry-run / --port
firmware-mod/  rt1000_patch.py — EL firmware modificable:
               parchea versión, fecha, textos y tablas de
               frecuencias del V1.16 y regenera un HEX válido
firmware/      firmware ARM Cortex-M en progreso (NO flashear,
               ver firmware/README.md): HAL abstracta, SystemInit
               documentado, receptor de update lado radio, parser
               NMEA, APRS y VFO; `make test` 21/21 en host
firmware-mod/  rt1000_patch.py — EL firmware modificable (ver abajo)
docs/          PROTOCOL.md — informe completo de ingeniería inversa
               FLASHING.md — como flashear (tests primero, app despues)
               RE-ANALYSIS-2026-11-17.md — re-análisis estático
               (M4F, 6 IRQs, GPS en IRQ, anomalía del PLL)
               analyze_v116.py — script reproducible del análisis
```

## Updater open source

```sh
cd updater
pip install -r requirements.txt

# Solo verificar el HEX (no toca el puerto)
python3 rt1000_update.py firmware.hex --verify-only

# Generar y verificar los 246 paquetes sin enviar nada
python3 rt1000_update.py firmware.hex --dry-run

# Flashear (radio en modo bootloader: PTT pulsado + encender)
python3 rt1000_update.py firmware.hex --port /dev/ttyUSB0
# en Windows: --port COM3
```

El updater **no incluye firmware propietario**. Aporta tu
propio `.hex` (por ejemplo, el extraído del actualizador
oficial para uso personal).

## Firmware modificable

Ver [`firmware-mod/README.md`](firmware-mod/README.md). Es la
forma de tener un firmware modificable hoy: `rt1000_patch.py`
edita versión, fecha, textos de menú y (con `--force-bands`,
sin verificar) tablas de frecuencias del V1.16, y regenera
un HEX válido para el updater. Ya está probado: un parche de
ejemplo cambia 13 bytes (versión `V1.16-XE1`, fecha y
`APRS Beacon` → `APRS XE1ARS`), SP/Reset intactos, dry-run OK.

## Firmware open source (desde cero)

Ver [`firmware/README.md`](firmware/README.md). Es un esqueleto:
mapa de memoria, vector table y receptor del protocolo de
update. Faltan, por confirmar por hardware: MCU exacto,
transceptor, display, pines, GPS/APRS. No genera un firmware
usable y no debe flashearse. Es el proyecto a largo plazo;
el patcher de `firmware-mod/` es el puente hasta entonces.

## Licencia

MIT — ver `LICENSE`. El código de este repo es original
(reimplementación limpia del protocolo observado). El firmware
oficial de Radtel sigue siendo propietario y no se incluye aquí.
