# Convertir el RT-1000Pro a DMR (NXDN / P25)

Objetivo de Lyra (2026-11-18): que el RT-1000Pro funcione en DMR.
Esto es el plan honesto, con lo que se puede hacer por firmware,
lo que falta por hardware y lo que NO se puede.

## La respuesta corta

El radio **no se convierte en un terminal DMR** (un handset que
hable DMR con su micrófono y bocina). Lo que sí se puede es
convertirlo en un **nodo MMDVM**: su parte de RF (BK4829) como
transceptor FM + un módem externo que hace el 4FSK y la pila
DMR/NXDN/P25. El vocoder vive en el otro radio, no en este.

## Por qué no hay port nativo (verificado)

- En el firmware V1.16: NXDN 0 ocurrencias, P25 0, DMR 1 (lista
  `ANA / DMR / EMP`). Nada de TDMA, AMBE, vocoder, Color Code,
  Talkgroup ni 4FSK.
- El BK4829 es un transceptor FM analógico: no tiene baseband
  digital ni módem 4FSK.
- El "codec" del DM32 no es un chip que se pueda trasplantar:
  el DM32 usa el SoC HR_C7000 (C-SKY), su módem DMR es hardware
  dentro de ese SoC y el codec AMBE es un blob de software que se
  copia a RAM al arrancar (propietario de DVSI; el flasher de
  Lyra lo toma de la memoria del propio radio en el backup).
  Sacarlo mataría el DM32 y no serviría en el 1000Pro (ARM,
  otro SoC, otra RF).
- Un vocoder AMBE en software sobre el Cortex-M4F + una pila
  DMR Tier II es un proyecto de meses, no un port, y seguiría
  necesitando los puntos de audio baseband (ver abajo).

## Arquitectura de la conversión (nodo MMDVM)

```
DMR/NXDN/P25 (RF) <-> BK4829 (FM) <-> audio plano <-> módem MMDVM <-> Pi/WPSD <-> BrandMeister
                                        |                 |
                                   DISC (RX)        PTT + COS
                                   MOD  (TX)
```

- El módem MMDVM hace la modulación/demodulación 4FSK y la
  pila de protocolo. Opciones: placa MMDVM con STM32, o
  Raspberry Pi + módem (el mismo ecosistema WPSD que Lyra ya
  usa con su hotspot).
- El radio solo pone: RF FM, PTT, audio RX sin procesar y
  entrada de modulación directa. No pone vocoder: la voz la
  codifica/decodifica el radio DMR del otro extremo (por
  ejemplo, su DM32).
- Resultado: un hotspot DMR/NXDN/P25 con la RF del 1000Pro.
  No un handset digital.

## Lo que ya hace el firmware (desde 2026-11-18)

- Menú **"Modo Modem"** (OFF/DMR/NXDN/P25) + pantalla propia.
- Al activarlo: ancho de banda WIDE, CTCSS/DCS a cero, squelch
  abierto (el módem decide por nivel), indicador `MDM` en la
  barra de estado. Al apagarlo se reaplican los ajustes
  guardados (los tonos y el ancho no se pierden).
- El scan queda bloqueado mientras el modo módem está activo.
- El protocolo elegido (DMR/NXDN/P25) es informativo: es lo
  que hay que habilitar en el MMDVMHost/WPSD del Pi.

## Lo que falta (hardware, con el radio abierto)

Hay que encontrar en la PCB estos cuatro puntos. Sin ellos el
modo módem no sirve de nada:

1. **DISC (RX plano):** audio del discriminador, sin
   de-énfasis ni filtro de audio ni squelch. Es la entrada del
   módem. Buscar con osciloscopio: señal de audio de ~300 Hz a
   3 kHz que sigue a la portadora aunque el squelch esté
   cerrado.
2. **MOD (TX plano):** entrada directa al modulador, sin
   pre-énfasis ni compresor de micrófono. Es la salida del
   módem. En el BK4829 el camino de micrófono pasa por
   filtros; hay que encontrar el punto antes del filtro o el
   pin de modulación directa.
3. **PTT:** la línea que el módem pone a masa para transmitir.
4. **COS / squelch:** señal de portadora presente (opcional si
   el módem usa solo nivel de audio, pero recomendable).

Cómo buscarlos: abrir el radio, localizar el BK4829 y su
alrededor, seguir las pistas de audio con multímetro y
osciloscopio, y medir con el radio en RX con portadora. Los
pines candidatos del BK4829 vienen del driver del RT-950 Pro
(`firmware/src/bk4829.c`); el mapa del 1000Pro NO está
confirmado. Documentar cada punto con foto y medida antes de
soldar nada.

## Puesta en marcha (cuando los puntos existan)

1. Flashear tests 3 y 4; solo si pasan, flashear la app.
2. Conectar DISC/MOD/PTT/COS al módem (cable corto, apantallado).
3. En el menú: Modo Modem → DMR (o NXDN/P25).
4. En el Pi (WPSD/MMDVMHost): habilitar el modo elegido, mismo
   Color Code / RAN / NAC que el resto de la red, y calibrar
   niveles RX/TX con el modo de calibración del MMDVM
   (la desviación 4FSK debe quedar en ±2.4 kHz aprox.).
5. Probar con el DM32 en simplex contra el nodo.

## Límites que no se quitan

- Sin vocoder no hay voz en el propio radio: el 1000Pro no
  puede ser el terminal que habla. Para hablar en DMR sigue
  siendo el DM32.
- El BK4829 es un solo transceptor: el nodo es simplex
  (una sola frecuencia, como un hotspot), no un repetidor
  dúplex, salvo que se use un segundo equipo para el otro lado.
- Airband TX sigue prohibido; DMR solo en bandas de aficionado
  (144-148 / 430-440 MHz) con el indicativo XE1ARS.
