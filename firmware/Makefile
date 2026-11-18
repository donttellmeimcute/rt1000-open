# Firmware open source RT-1000Pro / UV100 — COMPLETO, flasheable
# con el updater Python (rt1000_update.py) via el bootloader
# de fabrica. La app vive en 0x08002800 (246 KiB), el bootloader
# de fabrica (0x08000000-0x080027FF) NUNCA se toca.
#
# Hardware: MCU candidato AT32F403A (ver src/mcu.h). Los drivers
# de display / BK4829 / SI4732 / teclado son candidatos del
# RT-950 Pro, NO verificados en el 1000Pro. Antes de flashear
# la app, flashear un build de prueba de hardware:
#   make test-hw TEST=3   # LCD barras
#   make test-hw TEST=4   # BK4829 chip ID
# y confirmar cada periferico. Si un test falla, no flashear la app.
#
#   make test       # pruebas en host (gcc x86), 21+ checks
#   make arm        # build ARM -> build/rt1000-open.hex (flasheable)
#   make test-hw TEST=N  # build ARM de prueba de hardware
#   make size
TARGET   := rt1000-open
CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size
CFLAGS   := -mcpu=cortex-m4 -mthumb -Os -Wall -Wextra \
            -ffunction-sections -fdata-sections \
            -DRT1000_TARGET_ARM=1 -DRT1000_MCU_STM32F4LIKE_CANDIDATE=1 \
            -DSTM32F4_FAMILY_UNCONFIRMED=1
LDFLAGS  := -T linker.ld -nostartfiles -Wl,--gc-sections --specs=nosys.specs
SRCS := src/main.c src/app.c src/hw_test.c src/bootloader_protocol.c \
        src/bootloader_rx.c src/clock.c src/nmea.c src/aprs.c src/vfo.c \
        src/display.c src/ui.c src/bk4829.c src/si4732.c src/keypad.c src/storage.c \
        src/hal_mock.c src/hal_f4like.c src/uart_stub.c
OBJS := $(SRCS:.c=.o)

all: arm

arm: build/$(TARGET).elf build/$(TARGET).hex
	@echo "ADVERTENCIA: .hex generado. Antes de flashear, corre los"
	@echo "tests de hardware (make test-hw TEST=3, TEST=4...). Ver README.md"

build/$(TARGET).elf: $(OBJS) linker.ld
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

build/$(TARGET).hex: build/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

# El build de prueba recompila main.o con otro define, asi que
# limpia los objetos; al final reconstruye la app para no
# borrar build/rt1000-open.hex.
test-hw: CFLAGS += -DRT1000_HW_TEST=$(TEST)
test-hw: clean build/$(TARGET)-hwtest-$(TEST).hex
	@$(MAKE) --no-print-directory arm >/dev/null
	@echo "Build de prueba TEST=$(TEST) listo: build/$(TARGET)-hwtest-$(TEST).hex"

build/$(TARGET)-hwtest-%.hex: build/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

size: build/$(TARGET).elf
	$(SIZE) $<

# --- Pruebas en host (sin ARM) ---
HOST_CC := gcc
HOST_CFLAGS := -std=c11 -Wall -Wextra -Os -DRT1000_HOST_TEST=1 -Isrc
HOST_SRCS := tests/host_test.c src/bootloader_protocol.c src/bootloader_rx.c \
             src/clock.c src/nmea.c src/aprs.c src/vfo.c src/hal_mock.c \
             src/display.c src/bk4829.c src/si4732.c src/keypad.c src/storage.c
test: build/host_test
	./build/host_test

build/host_test: $(HOST_SRCS) src/*.h
	@mkdir -p build
	$(HOST_CC) $(HOST_CFLAGS) $(HOST_SRCS) -lm -o $@

# --- Emulador de display (host): renderiza las pantallas reales
# con el driver LCD y vuelca PNG en emu-out/ (ver tests/emu.c) ---
emu: build/emu
	@mkdir -p emu-out
	./build/emu
	@python3 -c "from PIL import Image; import glob; [Image.open(p).resize((480,640), Image.NEAREST).save(p.replace('.ppm','.png')) for p in sorted(glob.glob('emu-out/*.ppm'))]" 2>/dev/null || echo "PIL no disponible: quedaron solo los .ppm en emu-out/"

build/emu: tests/emu.c src/display.c src/ui.c src/hal_mock.c src/*.h
	@mkdir -p build emu-out
	$(HOST_CC) $(HOST_CFLAGS) tests/emu.c src/display.c src/ui.c src/hal_mock.c -o $@

clean:
	rm -rf build $(OBJS)

.PHONY: all arm test test-hw size clean
