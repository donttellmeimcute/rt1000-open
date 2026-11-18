# Firmware modificable — RT-1000Pro / UV100 V1.16

La idea no es reescribir 244 KB de ARM desde cero sin hardware.
Eso es un proyecto de meses y, sin el MCU y el transceptor
confirmados, terminaría en un brick.

La forma real de tener **un firmware modificable hoy** es esta:
parchear el firmware oficial en los datos que sí son editables
sin tocar código, regenerar un HEX válido y flashearlo con el
updater open source. Eso es lo que hace `rt1000_patch.py`.

## Qué puedes modificar

| Qué | Cómo | Riesgo |
|---|---|---|
| Versión en pantalla (`VER :V1.16`) | `"version": "V1.16-XE1"` (máx. 11 caracteres) | Bajo |
| Fecha (`DATE:2026-06-11`) | `"date": "2026-11-14"` (máx. 11 caracteres) | Bajo |
| Textos de menú / pantalla | `"strings": [{"old": "...", "new": "..."}]`, misma longitud o más corto | Bajo, si respetas la longitud |
| Tablas de frecuencias candidatas | `"bands": {"0x08007984": [...]}` + `--force-bands` | **Alto, sin verificar** |

El callsign APRS **no** está en el firmware: está en la
configuración del radio (CPS). No lo busques aquí.

## Uso

```sh
# 1. Ver qué textos hay (tu .hex, no incluido en el repo)
python3 rt1000_patch.py firmware.hex --list-strings | grep -i aprs

# 2. Ver las tablas de frecuencias candidatas
python3 rt1000_patch.py firmware.hex --list-bands

# 3. Parchear con tu config (copia config.example.json y edítala)
python3 rt1000_patch.py firmware.hex \
  --config mi-config.json --output firmware-XE1ARS.hex

# 4. Verificar sin flashear
python3 ../updater/rt1000_update.py firmware-XE1ARS.hex --verify-only
python3 ../updater/rt1000_update.py firmware-XE1ARS.hex --dry-run

# 5. Flashear (PTT pulsado + encender, como el updater oficial)
python3 ../updater/rt1000_update.py firmware-XE1ARS.hex --port /dev/ttyUSB0
```

El patcher comprueba antes de escribir: SP y Reset sin cambios,
tamaño 251904, HEX regenerado que re-parsea idéntico. Si algo
de eso falla, no escribe un HEX útil.

Para volver atrás: reflashea el `.hex` original. El bootloader
de fábrica (`0x08000000–0x080027FF`) no se toca nunca con este
flujo, así que el radio se puede recuperar por serie.

## Tablas de frecuencias: lo que se sabe y lo que no

En el firmware hay grupos de valores uint32 LE en unidades de
10 Hz que son frecuencias redondas en MHz:

- `0x08007984`: 136, 174, 400, 480, 18, 64, 45, 240, 320, 560, 660, 840, 920, 1000, 1080, 1160, 1240, 1300
- `0x08013A70`: 136, 174, 135, 5, 400, 480, 18, 64, 15, 60, 240, 170, 320, 260, 560, 660, 840, 920
- `0x08024DF4`: 64, 18, 136, 174, 240, 320, 400, 480, 560, 620, 920, 840, 1000
- `0x08008170`: 108, 136

Los primeros valores (136–174, 400–480) son los límites VHF/UHF
del equipo. El resto no se ha podido atribuir a una función
concreta desde el binario: no hay punteros directos a estas
tablas y el acceso parece ser por base + índice o por MOVW/MOVT.
Por eso el patcher exige `--force-bands` y el mismo número de
valores que la tabla original.

No cambies límites de TX a ciegas: transmitir fuera de las
bandas autorizadas para tu licencia (XE1ARS) es ilegal en México
y puede dañar el frontal del radio. Si investigas estas tablas,
hazlo primero en RX, con backup, y documenta qué cambia en
pantalla antes de transmitir nada.

## Fuera de alcance: DMR y Airband TX

- **DMR:** no se puede agregar por firmware. El V1.16 solo trae
  la cadena `DMR` una vez, sin TDMA, Color Code, Talkgroup ni
  módem 4FSK, y el hardware es analógico. Usa tu DM32 + hotspot
  MMDVM para DMR.
- **Airband TX:** no se desbloquea aquí. El firmware trae
  `Air TX On-off` y `Aviation transmission prohibited!
  License required!`, y Radtel solo lo habilita con licencia
  de aviación + AIR ID + contraseña de fábrica. RX en airband
  sí funciona y se queda como RX.

## El camino largo (firmware 100% open source)

El esqueleto en `../firmware/` sigue siendo el punto de partida
para un firmware escrito desde cero. Para llegar ahí falta, por
hardware: leer el marcado del MCU y su IDCODE por SWD, identificar
el transceptor (BK4819 / AT1846S / otro) y mapear display, teclado
y pines. Cuando eso esté, cada driver se porta y se prueba por
separado. El patcher de este directorio es el puente hasta entonces:
te deja personalizar el firmware que ya funciona, sin esperar
a la reescritura completa.
