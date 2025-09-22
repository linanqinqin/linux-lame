# linanqinqin
# Makefile for the lame-utils emulation tool

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 
TARGET = lame
SOURCE = lame.c
INSTALL_DIR = /usr/local/bin

# Default target
all: $(TARGET)

# Build the lame binary
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

# Install the binary to /usr/local/bin
install: $(TARGET)
	@echo "Installing $(TARGET) to $(INSTALL_DIR)..."
	@sudo cp $(TARGET) $(INSTALL_DIR)/
	@sudo chmod +x $(INSTALL_DIR)/$(TARGET)

# Uninstall the binary
uninstall:
	@echo "Removing $(TARGET) from $(INSTALL_DIR)..."
	@sudo rm -f $(INSTALL_DIR)/$(TARGET)

# Clean up compiled files
clean:
	rm -f $(TARGET)

# Show help
help:
	@echo "Available targets:"
	@echo "  all      - Build the lame binary (default)"
	@echo "  install  - Install lame to $(INSTALL_DIR)"
	@echo "  uninstall- Remove lame from $(INSTALL_DIR)"
	@echo "  clean    - Remove compiled files"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make              # Build the binary"
	@echo "  make install      # Build and install to system"
	@echo "  make clean        # Clean up build files"

# Declare phony targets
.PHONY: all install uninstall clean help
# end
