#Reference: https://makefiletutorial.com/

#Setting Compiler to Cross compiler enabling debug symbols
CC ?= $(CROSS_COMPILE)gcc
#Compiler Flags Remove any optimization to enable single stepping
CFLAGS := -Wall -Wextra -O0 -pthread
LDFLAGS := -pthread -lrt

TARGET = aesdsocket

#List of sources
SRCS = aesdsocket.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	
# Clean target
clean:
	rm -f $(TARGET) $(OBJS) /var/tmp/aesdsocketdata

.PHONY: all clean
