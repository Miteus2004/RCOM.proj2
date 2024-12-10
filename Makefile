# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Source and object files
SRC = download.c
OBJ = $(SRC:.c=.o)
TARGET = download

# Default target
all: $(TARGET)

# Link the object file to create the executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

# Compile the source file to an object file
$(OBJ): $(SRC) download.h
	$(CC) $(CFLAGS) -c $(SRC)

# Clean up the generated files
clean:
	rm -f $(OBJ) $(TARGET)
