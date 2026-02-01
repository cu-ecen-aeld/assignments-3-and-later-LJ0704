#Reference: https://makefiletutorial.com/

#Setting Compiler to Cross compiler
CC := $(CROSS_COMPILE)gcc

#Compiler Flags
CFLAGS := -Wall -Wextra -O2

TARGET = writer

#List of sources
SRCS = writer.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	
# Clean target
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
