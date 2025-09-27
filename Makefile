# Makefile for Nerd Fonts Installer

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Wformat-security -D_FORTIFY_SOURCE=2
LDFLAGS = -lcurl -ljansson

# Target executable name
TARGET = nerdfonts-installer
SOURCE = nerdfonts_installer.c

# Installation directory
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCE)
	@echo "Building $(TARGET)..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "Build complete!"

# Install the executable
install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)
	@echo "Installation complete!"

# Uninstall the executable
uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstall complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET)
	@echo "Clean complete!"

# Check if dependencies are installed
check-deps:
	@echo "Checking build dependencies..."
	@which $(CC) >/dev/null 2>&1 || (echo "Error: gcc not found. Please install build-essential or gcc." && exit 1)
	@pkg-config --exists libcurl || (echo "Error: libcurl development headers not found." && exit 1)
	@pkg-config --exists jansson || (echo "Error: libjansson development headers not found." && exit 1)
	@echo "All dependencies are satisfied!"

# Show help
help:
	@echo "Nerd Fonts Installer - Makefile Help"
	@echo "===================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build the executable (default)"
	@echo "  install    - Install to $(BINDIR)"
	@echo "  uninstall  - Remove from $(BINDIR)"
	@echo "  clean      - Remove build artifacts"
	@echo "  check-deps - Check if build dependencies are installed"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Build dependencies:"
	@echo "  - gcc (GNU Compiler Collection)"
	@echo "  - libcurl development headers"
	@echo "  - libjansson development headers"
	@echo ""
	@echo "Install dependencies by distribution:"
	@echo "  Arch Linux:    sudo pacman -S gcc make curl jansson"
	@echo "  Ubuntu/Debian: sudo apt-get install build-essential libcurl4-openssl-dev libjansson-dev"
	@echo "  Fedora:        sudo dnf install gcc make libcurl-devel jansson-devel"
	@echo "  CentOS/RHEL:   sudo yum install gcc make libcurl-devel jansson-devel"

# Declare phony targets
.PHONY: all install uninstall clean check-deps help
