# Compiler and Flags
CC      := gcc
CFLAGS  := -Wall -Wextra -O3

# Directories
SRC_DIR     := .
BUILD_DIR   := build
PREFIX      := $(HOME)/.local
BINDIR      := $(PREFIX)/bin

# Find all .c files in SRC_DIR
SRCS    := $(wildcard $(SRC_DIR)/*.c)

# Map each source file to an executable inside BUILD_DIR
# e.g., ./cpu_usage.c -> ./build/cpu_usage
TARGETS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%, $(SRCS))

# Default target: compile all executables
all: $(BUILD_DIR) $(TARGETS)

# Rule to compile a single .c file directly to its build/ executable
$(BUILD_DIR)/%: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

$(BUILD_DIR)/idleinhibitor: CFLAGS += $(shell pkg-config --cflags libsystemd)
$(BUILD_DIR)/idleinhibitor: LDLIBS += $(shell pkg-config --libs libsystemd)

# Create the build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# -----------------------------------------------------------------------------
# Installation Targets
# -----------------------------------------------------------------------------

# Ensure ~/.local/bin exists
$(BINDIR):
	mkdir -p $(BINDIR)

# Install ALL binaries to ~/.local/bin
install: all | $(BINDIR)
	install -m 755 $(TARGETS) $(BINDIR)/

# Install a SINGLE binary (Usage: make install-one TARGET=cpu_usage)
install-one: | $(BINDIR)
ifndef TARGET
	$(error Please specify a target to install, e.g., 'make install-one TARGET=cpu_usage')
endif
	@if [ ! -f $(BUILD_DIR)/$(TARGET) ]; then \
		echo "Building $(BUILD_DIR)/$(TARGET) first..."; \
		$(MAKE) $(BUILD_DIR)/$(TARGET); \
	fi
	install -m 755 $(BUILD_DIR)/$(TARGET) $(BINDIR)/
	@echo "Successfully installed $(TARGET) to $(BINDIR)/"

# Uninstall all binaries compiled from this directory
uninstall:
	@for target in $(notdir $(TARGETS)); do \
		echo "Removing $(BINDIR)/$$target"; \
		rm -f $(BINDIR)/$$target; \
	done

# Clean up build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean install install-one uninstall
