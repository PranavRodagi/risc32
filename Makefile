# RISC-32 Virtual CPU — Makefile

CC      = gcc
CFLAGS  = -Wall -Wextra -I include -I tools
SRC     = src/main.c src/cpu.c tools/assembler.c tools/debugger.c
TARGET  = risc32

# default target — builds the project
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

# run with the counting program
run-count:
	$(MAKE) all
	./$(TARGET) tests/count.asm

# run with the security program  
run-security:
	$(MAKE) all
	./$(TARGET) tests/security.asm

# remove build artifacts
clean:
	rm -f $(TARGET)

.PHONY: all clean run-count run-security