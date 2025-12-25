# Makefile for Nerd Fonts Installer (Security Hardened)

# Compiler and flags
CC = gcc

# Base compilation flags
BASE_CFLAGS = -Wall -Wextra -Wpedantic -std=c11

# Security hardening flags
SECURITY_CFLAGS = -Wformat -Wformat-security \
                  -D_FORTIFY_SOURCE=2 \
                  -fstack-protector-strong \
                  -fPIE \
                  -D_GLIBCXX_ASSERTIONS

# Linker security flags
SECURITY_LDFLAGS = -pie \
                   -Wl,-z,relro,-z,now \
                   -Wl,-z,noexecstack

# Optimization flags (can be overridden)
OPT_FLAGS = -O2

# Combined flags
CFLAGS = $(BASE_CFLAGS) $(SECURITY_CFLAGS) $(OPT_FLAGS)
LDFLAGS = -lcurl -ljansson $(SECURITY_LDFLAGS)

# Target executable name
TARGET = nerdfonts-installer
SOURCE = nerdfonts_installer.c

# Installation directory
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

# Build modes
DEBUG_CFLAGS = -g -O0 -DDEBUG
RELEASE_CFLAGS = -O3 -DNDEBUG

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(SOURCE)
	@echo "Building $(TARGET) with security hardening..."
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "Build complete!"
	@echo "Security features enabled:"
	@echo "  ✓ Stack protection (-fstack-protector-strong)"
	@echo "  ✓ Position Independent Executable (-fPIE -pie)"
	@echo "  ✓ Full RELRO (-z,relro,-z,now)"
	@echo "  ✓ Format string protection (-Wformat-security)"
	@echo "  ✓ Fortified source (-D_FORTIFY_SOURCE=2)"
	@echo "  ✓ Non-executable stack (-z,noexecstack)"

# Debug build with symbols and no optimization
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: clean $(TARGET)
	@echo "Debug build complete with symbols!"

# Release build with maximum optimization
release: CFLAGS := $(BASE_CFLAGS) $(SECURITY_CFLAGS) $(RELEASE_CFLAGS)
release: clean $(TARGET)
	@echo "Release build complete with optimizations!"

# Static analysis with additional warnings
analyze: CFLAGS += -Wconversion -Wshadow -Wcast-align -Wunused \
                   -Wcast-qual -Wfloat-equal -Wpointer-arith
analyze: clean $(TARGET)
	@echo "Static analysis build complete!"

# Install the executable
install: $(TARGET)
	@echo "Installing $(TARGET) to $(BINDIR)..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Warning: You may need sudo privileges for installation"; \
	fi
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)
	@echo "Installation complete!"
	@echo "Run with: $(TARGET)"

# Uninstall the executable
uninstall:
	@echo "Removing $(TARGET) from $(BINDIR)..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "Warning: You may need sudo privileges for uninstallation"; \
	fi
	rm -f $(BINDIR)/$(TARGET)
	@echo "Uninstall complete!"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET) $(TARGET).o
	@echo "Clean complete!"

# Check if dependencies are installed
check-deps:
	@echo "Checking build dependencies..."
	@which $(CC) >/dev/null 2>&1 || (echo "Error: gcc not found. Please install build-essential or gcc." && exit 1)
	@pkg-config --exists libcurl || (echo "Error: libcurl development headers not found." && exit 1)
	@pkg-config --exists jansson || (echo "Error: libjansson development headers not found." && exit 1)
	@echo "✓ All dependencies are satisfied!"

# Verify security features in the compiled binary
verify-security: $(TARGET)
	@echo "Verifying security features in $(TARGET)..."
	@echo ""
	@echo "Checking for PIE (Position Independent Executable):"
	@if command -v hardening-check >/dev/null 2>&1; then \
		hardening-check $(TARGET); \
	elif command -v checksec >/dev/null 2>&1; then \
		checksec --file=$(TARGET); \
	elif readelf -h $(TARGET) 2>/dev/null | grep -q "Type.*DYN"; then \
		echo "  ✓ PIE enabled"; \
	else \
		echo "  Note: Install 'hardening-check' or 'checksec' for detailed analysis"; \
		echo "  Attempting basic check with readelf..."; \
		readelf -h $(TARGET) 2>/dev/null | grep Type || echo "  readelf not available"; \
	fi
	@echo ""
	@echo "Checking for stack canary:"
	@if readelf -s $(TARGET) 2>/dev/null | grep -q "__stack_chk_fail"; then \
		echo "  ✓ Stack canary enabled"; \
	else \
		echo "  Note: Stack canary symbols not found in binary"; \
	fi
	@echo ""
	@echo "Checking for RELRO:"
	@if readelf -lW $(TARGET) 2>/dev/null | grep -q "GNU_RELRO"; then \
		echo "  ✓ RELRO enabled"; \
	else \
		echo "  Note: RELRO not detected"; \
	fi
	@echo ""
	@echo "Checking for NX (Non-Executable Stack):"
	@if readelf -lW $(TARGET) 2>/dev/null | grep "GNU_STACK" | grep -q "RW"; then \
		echo "  ✓ NX enabled (non-executable stack)"; \
	else \
		echo "  Note: Could not verify NX bit"; \
	fi

# Run basic tests (if you add test functionality later)
test: $(TARGET)
	@echo "Running basic validation..."
	@./$(TARGET) --help 2>/dev/null || echo "Note: Add --help flag for complete testing"
	@echo "Basic validation complete!"

# Generate compilation database for IDE/tools
compile_commands:
	@echo "Generating compile_commands.json..."
	@echo '[' > compile_commands.json
	@echo '  {' >> compile_commands.json
	@echo '    "directory": "'$$(pwd)'",' >> compile_commands.json
	@echo '    "command": "$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)",' >> compile_commands.json
	@echo '    "file": "$(SOURCE)"' >> compile_commands.json
	@echo '  }' >> compile_commands.json
	@echo ']' >> compile_commands.json
	@echo "compile_commands.json generated!"

# Show detailed build information
info:
	@echo "Build Configuration"
	@echo "==================="
	@echo "Compiler:      $(CC)"
	@echo "CFLAGS:        $(CFLAGS)"
	@echo "LDFLAGS:       $(LDFLAGS)"
	@echo "Target:        $(TARGET)"
	@echo "Source:        $(SOURCE)"
	@echo "Install Path:  $(BINDIR)"
	@echo ""
	@echo "Security Features:"
	@echo "  - Stack Protector:     -fstack-protector-strong"
	@echo "  - PIE:                 -fPIE -pie"
	@echo "  - RELRO:               -Wl,-z,relro,-z,now"
	@echo "  - Format Security:     -Wformat-security"
	@echo "  - Fortify Source:      -D_FORTIFY_SOURCE=2"
	@echo "  - No Exec Stack:       -Wl,-z,noexecstack"

# Show help
help:
	@echo "Nerd Fonts Installer - Makefile Help"
	@echo "===================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all             - Build the executable (default) with security hardening"
	@echo "  debug           - Build with debug symbols and no optimization"
	@echo "  release         - Build with maximum optimization"
	@echo "  analyze         - Build with extra static analysis warnings"
	@echo "  install         - Install to $(BINDIR)"
	@echo "  uninstall       - Remove from $(BINDIR)"
	@echo "  clean           - Remove build artifacts"
	@echo "  check-deps      - Check if build dependencies are installed"
	@echo "  verify-security - Verify security features in compiled binary"
	@echo "  test            - Run basic validation tests"
	@echo "  compile_commands- Generate compile_commands.json for IDEs"
	@echo "  info            - Show detailed build configuration"
	@echo "  help            - Show this help message"
	@echo ""
	@echo "Security Features (enabled by default):"
	@echo "  ✓ Stack protection against buffer overflows"
	@echo "  ✓ Position Independent Executable (PIE)"
	@echo "  ✓ Full RELRO (RELocation Read-Only)"
	@echo "  ✓ Non-executable stack (NX bit)"
	@echo "  ✓ Format string protection"
	@echo "  ✓ Fortified source (buffer overflow detection)"
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
	@echo ""
	@echo "Optional security verification tools:"
	@echo "  Ubuntu/Debian: sudo apt-get install hardening-check"
	@echo "  Arch Linux:    yay -S checksec"
	@echo "  Fedora:        sudo dnf install checksec"

# Declare phony targets
.PHONY: all debug release analyze install uninstall clean check-deps \
        verify-security test compile_commands info help
