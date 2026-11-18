# Funciones del firmware original V1.16 — estado del port

Inventario hecho a partir de las cadenas del firmware V1.16
(`rt1000-re/firmware.bin`) y de `docs/PROTOCOL.md` § Funciones.
Estado a 2026-11-18. "Portado" = implementado y probado en host
(`make test`); "pendiente HW" = necesita confirmación del chip o
del pin en el radio real antes de darse por válido.

## Portado y probado en host

| Función original | Estado en el firmware open |
|---|---|
| VFO A/B, paso 5/6.25/10/12.5/25 kHz | Portado (`vfo.c`, `app.c`) |
| Modos FM/AM/USB/LSB/CW | Portado (menú Modo) |
| Potencia Baja/Media/Alta | Portado (BK4829 REG_36) |
| Squelch 0-9, S-meter | Portado (RSSI BK4829 REG_0C) |
| Entrada directa de frecuencia | Portado (`*` = punto decimal) |
| GPS Long/Lat/Alti/Course/Speed | Portado (parser NMEA RMC/GGA, sats de GGA) |
| GPS formato grado / grado.min / grado.min.seg | Portado (menú GPS formato) |
| System Time (hora GPS) | Portado (pantalla GPS) |
| APRS beacon, SSID, Path1-5 | Portado (frame AX.25 con path, `aprs.c`) |
| APRS Detail, FCS Check | Portado (pantalla APRS, FCS X.25 verificado) |
| Call Locate | Portado (pantalla APRS) |
| APRS símbolos (tabla en flash) | Tabla presente en V1.16; símbolo por defecto "radioaficionado" |
| TNC Stream | Portado (frame por UART cuando TNC=ON) |
| Scanning + Scan Interval | Portado (paso con wrap por banda, parada por squelch) |
| NOAA CH (WX1-WX7, 162.400-162.550) | Portado (tabla en `vfo.c`, solo RX) |
| Radio FM broadcast (SI4732) | Portado (pantalla FM 87.5-108 MHz, init en arranque) |
| Shift / Offset / Talkaround / Reverse | Portado (cálculo TX, Prohibit TX fuera de banda) |
| CTCSS 48 tonos / DCS 104 códigos | Portado (tablas `tones.c`, menú CTCSS RX/TX, se guardan por canal) |
| DTMF (códigos + frecuencias) | Tablas y código configurables (menú DTMF); audio por DAC pendiente |
| MDC ID | ID configurable (menú MDC ID); codificación MDC1200 pendiente |
| VOX 0-9 | Menú + selección guardada; registro BK4829 sin confirmar |
| Ancho de banda Wide/Narrow | Menú + selección guardada; registro BK4829 sin confirmar |
| Tone Burst 1750 Hz | Menú + selección guardada; registro BK4829 sin confirmar |
| Monitor Mode | Portado (squelch abierto, indicador MON) |
| Modo Módem (DMR/NXDN/P25) | Portado: menú OFF/DMR/NXDN/P25 + pantalla; WIDE, CTCSS/DCS off, squelch abierto, indicador MDM. Es el lado firmware de la conversión a nodo MMDVM — ver `docs/DMR-CONVERSION.md`. El módem y los puntos de audio plano son hardware, no firmware. |
| Multi Standby | Menú (config, doble VFO ya visible) |
| TX Priority (A/B/OFF) | Menú (config) |
| Zonas (Zone Set) | Portado (4 zonas x 4 canales, nombres editables en flash) |
| Canales 16 slots | Portado (RAM + persistencia en flash 25Q, último sector) |
| Backup ("Backing up... / Please wait") | Portado (menú Backup, sector con magic+checksum) |
| Radio Reset | Portado (menú Reset, con confirmación, vuelve a defaults) |
| Device Name / Edit Name | Portado (editor de nombre, 12 caracteres) |
| Callsign (editor) | Portado (editor, 6 caracteres A-Z0-9) |
| BATT:0.0V | Pantalla Info (ADC sin confirmar, muestra --.-V) |
| Tema rosa KAWAII / ORIGINAL | Portado (menú Tema, default KAWAII) |
| Detección flash 25Q80-25Q512 | Portado (`flash25q_detect`, TEST 6) |

## Pendiente de hardware (no afirmable sin el radio)

| Función | Bloqueo |
|---|---|
| APRS TX por AFSK | El frame se construye (con FCS), pero el camino de audio (DAC → BK4829) no está confirmado. |
| DTMF/MDC TX por audio | Igual que APRS: falta el camino de audio. |
| VOX / Wide-Narrow / Tone Burst / CTCSS al chip | Las selecciones se guardan y se muestran, pero los registros BK4829 exactos no están documentados en la referencia del RT-950. TEST 4 + analizador lógico. |
| Batería real | ADC1 existe en V1.16 (IRQ 18), pero el canal del divisor no está confirmado. TEST 7. |
| SI4732 real | Driver listo; presencia por confirmar con TEST 5. |
| Flash 25Q real | Driver listo (último sector); tamaño por confirmar con TEST 6. |
| Frecuencia BK4829 (PLL 10 Hz) | El cálculo depende del cristal de referencia, sin confirmar. |
| Reloj real (PLL 192 MHz) | El valor de SystemInit es inválido en STM32F4; ver RE-ANALYSIS §3. |

## No se portan (decisiones con evidencia)

| Función | Razón |
|---|---|
| DMR | El hardware es analógico; "DMR" aparece una sola vez en V1.16 sin TDMA/modem. El camino DMR sigue siendo el Baofeng DM32. |
| NXDN / P25 | No aparecen ni una vez en V1.16 (0 ocurrencias); tampoco TDMA, AMBE, vocoder, Color Code, Talkgroup ni 4FSK. Sin código de voz digital. |
| Trasplante del codec del DM32 | No hay chip que sacar: en el DM32 el codec AMBE es un blob de software que se copia a RAM al arrancar, y el módem DMR es hardware dentro del SoC HR_C7000 (C-SKY, no ARM). Sacarlo mataría el DM32 y no serviría en el 1000Pro. La vía real para modos digitales en este radio sigue siendo un módem MMDVM externo. |
| Airband TX | Prohibido por fábrica ("Aviation transmission prohibited! License required!"); solo con licencia de aviación + Radtel. Airband queda RX. |
| All Band Test Mode | Modo de fábrica, no es función de usuario. |
| DTMF Remote Kill / Remote CTC-DCS | Requieren decodificación DTMF por DSP en recepción, no implementada. |
| Emergency Alarm TX | La pantalla y el tono están; la transmisión real depende del PTT, sin cablear. |

## Cómo se verifica

- `make test` — lógica en host (NMEA, APRS+FCS, TX freq, NOAA, scan, tonos, persistencia con flash mock, menú con scroll).
- `make emu` — 17 pantallas renderizadas con el driver LCD real en `firmware/emu-out/`.
- `make arm` + `updater --verify-only` + `--dry-run` — .hex válido (246 paquetes).
- Antes de flashear la app: `make test-hw TEST=3` (LCD) y `TEST=4` (BK4829). Ver `docs/FLASHING.md`.
