# Ingeniería inversa — Radtel RT-1000Pro Update 260611

Fecha del análisis: 2026-11-14.
Archivo analizado: `Radtel_RT-1000Pro_Update_260611_3.exe`
SHA-256: `92a0d1cd7610d45c3abef16d7eef1977d8b7d4cb2f5c5362f62fa6d76a2efb66`
Tamaño: 1.403.392 bytes.

## 1. Qué es el archivo

No es el firmware en sí: es el **actualizador oficial** para Windows.

- PE32, ensamblado .NET Framework 4.0 Client Profile, WinForms.
- Assembly: `Ido_Update`, versión 1.0.0.0, Company `china`,
  Copyright `© china 2018`, GUID `290da0ac-1364-4d3c-82cb-e2aa2331cc6f`.
- Ruta de compilación filtrada en el PDB:
  `E:\PCCode\Update\UV100\UV100_Radtel\Ido_Update\obj\x86\Debug\Ido_Update.pdb`
- Título de la ventana: `Radtel_RT-1000Pro_Update 260611`.
- Instrucciones en la ventana:
  1. Connect the data cable and select the port
  2. Press and hold the PTT key, then turn on the radio
  3. Click the upgrade button
- Recursos embebidos: solo icono y dos bitmaps de botones.
  El firmware **no** está en un recurso: está en una cadena
  `allcode` dentro del código (15.242 registros Intel HEX).

Desensamblado con ILSpy 9.1 (`ilspycmd`), verificado a mano
contra el IL y contra los bytes del HEX.

## 2. Firmware extraído

- Formato: Intel HEX, en la cadena `allcode` (655.231 caracteres).
- Registros: **15.242, todos con checksum válido**.
- Primer registro: `:020000040800F2` (Extended Linear Address `0x0800`).
- Último registro de datos: `:0CE0300030F25A022260D2FF13127A0074`
  (dirección `0xE030`, 12 bytes).
- Registro de entry: `:04000005080029E1E5` (Start Linear Address
  `0x080029E1`).
- Rango: **`0x08002800 – 0x0803E03B`, contiguo, 243.772 bytes**.
- SHA-256 del binario reconstruido (243.772 bytes):
  `85cd616cfe1b45760c241c009e036a9f05cecfe1d8be78a9bfaa47c47342c485`
- Versión dentro del firmware: `VER :V1.16`, `DATE:2026-06-11`
  (coincide con `260611` del nombre del actualizador).
- Cadena `UV100` en `0x0802B7B9`: la plataforma es la UV100;
  el RT-1000Pro es su variante / rebrand.

### Mapa de memoria

| Región | Dirección | Tamaño |
|---|---|---|
| Flash total (inferido) | `0x08000000–0x0803FFFF` | 256 KiB |
| Bootloader de fábrica (no está en el HEX) | `0x08000000–0x080027FF` | 10 KiB |
| Aplicación (este firmware) | `0x08002800–0x0803FFFF` | 246 KiB |
| Imagen que envía el updater | `0x08002800` + 251.904 bytes | 246 KiB, padding a `0x00` |

La imagen enviada es de 251.904 bytes (246 × 1024). Los últimos
8.132 bytes tras el final real (`0x0803E03B`) son `0x00`, porque
el updater inicializa su buffer `hex[251904]` a cero.

### Vector table / CPU

- SP inicial: `0x2000AEC8`
- Reset: `0x08002AC1` (Thumb)
- NMI: `0x0801C2A9`, HardFault: `0x08015155`, etc.
- 57 de 64 vectores no nulos.
- Reset_Handler es un trampolín de 2 instrucciones:
  `LDR r0, [PC, #0x24]; BLX r0` → init en `0x08022FA9`,
  `LDR r0, [PC, #0x24]; BX r0` → entry en `0x080029E1`.
- CPU: ARM Cortex-M, Thumb-2. No se pudo confirmar el modelo
  exacto solo por el binario.

### Periféricos (mapa compatible STM32F4 / GD32F4)

Direcciones que aparecen como literales en el firmware:

- GPIOA `0x40020000` (44 refs), GPIOB `0x40020400`,
  GPIOC `0x40020800`, GPIOD `0x40020C00`
- RCC `0x40023800`
- USART1 `0x40011000`, USART6 `0x40011400`,
  USART2 `0x40004400`, USART3 `0x40004800`, UART4 `0x40004C00`
- SPI2 `0x40003800`, SPI3 `0x40003C00`
- I2C2 `0x40005800`, TIM, DAC `0x40007400`, etc.

Esto es **compatible** con STM32F4, pero también con clones
pin-compatibles (GD32F4, AT32F4, APM32F4). No distingue el
fabricante: hay que leer el marcado del chip y el DBGMCU IDCODE
por SWD. No afirmar un modelo concreto sin esa lectura.

### Funciones (cadenas en el firmware)

APRS (con SSID, Path1-5, FCS Check, Call Locate), GPS
(Long/Lat/Alti/Course/Speed), DTMF, MDC, SSB/AM/FM, NOAA CH,
VOX, Talkaround, Reverse Freq, Emergency Alarm, Tone Burst,
Multi Standby, TX Priority, Scanning, Bandwidth Wide/Narrow,
detección de SPI flash externa `25Q80 1MB … 25Q512 64MB`,
`BATT:0.0V`, `Backing up... / Please wait`, `Radio restarting`,
`GPS not located`, `is out of range`, `Invalid Key`.

No hay ninguna cadena que nombre el transceptor
(BK4819 / AT1846S / etc.) ni el MCU. Esos dos datos son el
siguiente paso de RE por hardware, no por este binario.

## 3. Protocolo serie (lado PC → radio)

Puerto: **115200 8N1, DTR = true**. `SerialPort` .NET por defecto
(8 bits, sin paridad, 1 stop). El puerto se elige en un combo;
el botón de reset solo re-escanea puertos.

### Paquetes fijos

```
CONNECT  39 33 05 10 28   "93" 05 10 28
UPDATE   39 33 05 55 6D   "93" 05 55 6D
END      39 33 05 EE 06   "93" 05 EE 06   <- definido, NUNCA se envía
ACK      06
ERROR    FF
IGNORE   32  ('2', se ignora)
```

`Sendbuf_Right = {0x06}` y `Sendbuf_Error = {0xFF}` están
definidos pero el código solo compara el byte recibido.

### Paquete de datos (1028 bytes)

```
[0]    0x57  'W'
[1]    offset >> 8
[2]    offset & 0xFF
[3..1026]  1024 bytes de imagen
[1027] checksum
```

Checksum (`checksum()` en el original):

```
sum = 0
for i in 0 .. 1026: sum += Sendbuf[i]
sum += 82   # 'R'
sum += 84   # 'T'
return (sum + 1) & 0xFF
```

Es decir: suma de los primeros 1027 bytes + `'R' + 'T' + 1`, módulo 256.

### Secuencia (máquina de estados)

```
btn_update_Click:
  abrir puerto, step = 1, sendcnt = 0
  enviar CONNECT

comm_DataReceived (1 byte por evento):
  si byte == 0x00: sendcnt = 0, dormir 200 ms, no procesar
  si no: RevDateOperation()

RevDateOperation (solo si recvcnt == 1):
  0xFF -> cerrar puerto, step = 0, "Communication Error!"
  0x32 -> ignorar
  0x00 -> recvcnt = 0
  0x06 (ACK):
    step < 3  -> step++, reenviar CONNECT, dormir 50 ms
    step == 3 -> enviar UPDATE, dormir 10 ms, step = 4
    step == 4 -> enviar siguiente paquete de 1024 bytes
                 (Sendbuf[1..2] = sendcnt, checksum en [1027]),
                 dormir 10 ms, sendcnt += 1024
                 si sendcnt == 251904: step = 5, "Success!",
                 cerrar puerto
```

En total en el cable: **4 × CONNECT, 1 × UPDATE, 246 × datos
(1028 bytes) = 251 escrituras, 252.913 bytes**. Cada paquete
de datos espera su ACK `0x06` antes de enviar el siguiente.
No hay timeout ni reintento en el original: si se pierde un ACK,
se queda colgado. El updater open source añade timeout de 5 s
y aborta en ese caso (sin cambiar el formato en el cable).

### Parsing del HEX (stringopration)

El original recorre `allcode` buscando `:`, y por registro:

- tipo `0x04` (ELA): `writestep++` — **ignora el valor del ELA**,
  solo cuenta cuántos hay.
- tipo `0x00` (datos): dirección `num3`, y escribe en
  `hex[num3 + j - 10240 + (writestep - 1) * 65536]`
  (`10240 = 0x2800`). Para este firmware funciona porque el
  único ELA es `0x0800` y llega primero.
- tipo `0x01` (EOF): termina.
- otros tipos: se ignoran.

El updater open source implementa el ELA correctamente y
valida el rango; si un HEX usara otro ELA, lo rechaza en vez
de escribir fuera de sitio.

### Código muerto (no se usa en el update)

- `DesClass`: DES con clave `comehere` (`DESSetKey` añade
  c-o-m-e-h-e-r-e), RSA con contenedor `rsa_hs`
  (`RSAEncryption` / `RSADecrypt` / `DESEncrypt` / `DESDecrypt`).
  Ningún método de `UpDate` lo llama. Probablemente es código
  heredado del software CPS.
- `GenerateCheckCode`: genera códigos aleatorios alfanuméricos,
  no se llama.
- `timer1_Tick`: vacío.
- `DataSum`: suma `array[0 .. array[2]-2]`, no se llama en el
  flujo de update (el checksum que sí se usa es `checksum()`).

## 4. Qué NO se verificó

- No hay hardware aquí: no se flasheó nada, no se capturó
  tráfico serie real con el radio, no se leyó el bootloader
  de fábrica (`0x08000000–0x080027FF` no está en el HEX).
- El comportamiento del bootloader ante paquetes corruptos,
  offsets fuera de orden o un CONNECT extra no se observó:
  solo se infiere del código del updater.
- El MCU exacto, el transceptor, el display y los pines
  no se pueden determinar desde este .exe. Siguiente paso:
  abrir el radio, leer marcados, SWD + analizador lógico.

## 5. Qué es modificable (firmware-mod)

Sin reescribir código, en el firmware V1.16:

- `VER :V1.16      ` en `0x0800D234` (16 bytes, valor máx. 11 car.)
- `DATE:2026-06-11 ` en `0x0800D24C` (16 bytes, valor máx. 11 car.)
- Cualquier cadena ASCII, misma longitud o más corta (3.731
  cadenas >= 4 car. en el binario; 1.222 terminan en espacio,
  típico de campos de pantalla de ancho fijo).
- Tablas candidatas uint32 LE en unidades de 10 Hz, sin verificar
  su función: `0x08007984`, `0x08013A70`, `0x08024DF4`, `0x08008170`.
  La primera empieza 136, 174, 400, 480 MHz (límites VHF/UHF).
  El patcher (`../firmware-mod/rt1000_patch.py`) las lista,
  las parchea solo con `--force-bands` y verifica SP/Reset,
  tamaño y re-parse del HEX. Un parche de ejemplo (versión,
  fecha y `APRS Beacon` → `APRS XE1ARS`) cambia 13 bytes exactos.
- No modificable con el patcher: vector table, bootloader,
  código ARM, ni el callsign APRS (está en la config del radio,
  no en el firmware).

## 6. DMR y Airband TX: lo que se pidió y por qué no va en este proyecto

Preguntado por Lyra el 2026-11-14: agregar DMR y desbloquear Air TX.

**DMR: no viable por firmware.**
- Specs oficiales del RT-1000Pro no listan DMR (sí lo hacen
  otros modelos Radtel como RT-7D y RT-6D PRO).
- En el firmware V1.16 la cadena `DMR` aparece **una sola vez**,
  en `0x08022309`, dentro de una lista `ANA / DMR / EMP` de 8 bytes
  por entrada. No hay `TDMA`, `Color Code`, `Talkgroup`, vocoder
  ni código de módem 4FSK en todo el binario.
- DMR necesita vocoder AMBE+2, módem 4FSK y timing TDMA, y un
  transceptor con baseband digital. Este equipo es analógico
  (FM/AM/SSB). Parchear una cadena no crea un módem DMR.
- Vía práctica: Lyra ya tiene DMR en su Baofeng DM32 + hotspot
  MMDVM / BrandMeister. El RT-1000Pro queda para analógico/APRS.

**Airband TX (108–136 MHz AM): no se desbloquea en este proyecto.**
- El firmware trae el menú `Air TX On-off` y la cadena en
  `0x080073C7`: `Aviation transmission prohibited!    License required!`
- Radtel lo desactiva de fábrica para cumplir la regulación de
  aviación. La única vía oficial es: enviar a info@radtel.com
  número de pedido, foto de licencia de aviación válida y el
  AIR ID del radio (Basic Setting → 44 Version → AIR Number);
  Radtel verifica y entrega la contraseña del equipo.
  Fuente: página oficial del RT-1000Pro, sección
  "Air Band Transmission Regulation".
- La licencia de radioaficionado (XE1ARS) no cubre la banda
  aérea (servicio móvil aeronáutico, 118–136 MHz). Transmitir
  ahí puede interferir torre/avión y es ilegal y peligroso.
  La recepción en airband (108–136 MHz AM RX) ya funciona y
  se deja como RX.
- Por eso el patcher no incluye ninguna opción para desbloquear
  Air TX, y no se buscará el bypass de la contraseña de fábrica.

## 7. Archivos de trabajo (no incluidos en el repo público)

En la máquina de análisis quedaron, para uso personal:

- `firmware.hex` (655.231 caracteres, 15.242 registros)
- `firmware.bin` (243.772 bytes, SHA-256 arriba)
- `decompiled/Radtel_RT-1000Pro_Update_260611_3.decompiled.cs`
  (desensamblado ILSpy del updater, 731 líneas)

El firmware extraído es propietario de Radtel: **no se
redistribuye en este repo**. El updater open source carga
un `.hex` aportado por el usuario.
