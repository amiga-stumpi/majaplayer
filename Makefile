CC = /opt/amiga/bin/m68k-amigaos-gcc
AS = /opt/amiga/bin/vasmm68k_mot
CFLAGS ?= -O2 -Wall -Wextra -mcrt=nix13 -DAMITCP13_OS13
INCLUDES = -I/opt/amitcp13/include -I/opt/amiga-netinclude/include

BUILD_DIR = build
TARGET = $(BUILD_DIR)/MajaRadio
SOURCES = src/main.c
ASM_OBJECTS = $(BUILD_DIR)/ptplayer_wrapper.o

.PHONY: all clean debug

all: $(TARGET)

debug: CFLAGS += -DMAJAPLAYER_DEBUG=1
debug: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SOURCES) $(ASM_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SOURCES) $(ASM_OBJECTS)

$(BUILD_DIR)/ptplayer_wrapper.o: src/ptplayer_wrapper.asm thirdparty/minimod/ptplayer.asm thirdparty/minimod/ptplayer-oscompat.asm | $(BUILD_DIR)
	$(AS) -Fhunk -kick1hunks -quiet -Ithirdparty/minimod -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
