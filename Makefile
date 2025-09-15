CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lcurl -ljansson
TARGET = nerdfonts-installer

all: $(TARGET)

$(TARGET): nerdfonts_installer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
