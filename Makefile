CC = /opt/amiga/bin/m68k-amigaos-gcc
CFLAGS ?= -O2 -Wall -Wextra -mcrt=nix13 -DAMITCP13_OS13
INCLUDES = -I/opt/amitcp13/include -I/opt/amiga-netinclude/include

BUILD_DIR = build
TARGET = $(BUILD_DIR)/MajaPlayer
SOURCES = src/main.c

.PHONY: all clean

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SOURCES)

clean:
	rm -rf $(BUILD_DIR)
