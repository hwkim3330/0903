# Makefile for LAN9692 CBS Implementation

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lrt -lpthread
TARGET = lan9692_cbs_test
OBJECTS = main.o lan9692_cbs.o

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
main.o: main.c lan9692_cbs.h
	$(CC) $(CFLAGS) -c main.c -o main.o

lan9692_cbs.o: lan9692_cbs.c lan9692_cbs.h
	$(CC) $(CFLAGS) -c lan9692_cbs.c -o lan9692_cbs.o

# Run test scenarios
test: $(TARGET)
	@echo "Running CBS Test Scenarios..."
	@echo "Scenario 1: CBS Disabled"
	sudo ./$(TARGET) 1
	@echo ""
	@echo "Scenario 2: CBS Enabled"
	sudo ./$(TARGET) 2
	@echo ""
	@echo "Scenario 3: High Bandwidth Reservation"
	sudo ./$(TARGET) 3

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install (requires root)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Debug build
debug: CFLAGS += -DDEBUG -g3
debug: clean $(TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all     - Build the CBS test application"
	@echo "  test    - Run all test scenarios"
	@echo "  clean   - Remove build artifacts"
	@echo "  install - Install to /usr/local/bin"
	@echo "  debug   - Build with debug symbols"
	@echo "  help    - Show this help message"

.PHONY: all test clean install debug help