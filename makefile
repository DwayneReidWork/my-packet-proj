CC = gcc
CFLAGS = -Wall -g

# Source and header files
SRCS = server/src/main.c server/src/server.c
HDRS = server/inc/server.h

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
EXEC = my_server

# Default rule
all: $(EXEC)

# Link object files to create executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile .c files to .o
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(EXEC) $(OBJS)

.PHONY: all clean
