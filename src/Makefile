# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11 -pthread
LDFLAGS = -pthread

# Project files
TARGET = storage_daemon
SRCS = main.c hardware.c display.c server.c mongoose.c qrcodegen.c
OBJS = $(SRCS:.c=.o)

# Default rule
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleanup
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean