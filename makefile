CC = gcc --std gnu99

CFLAGS = -Wall -g

# Source and header files
SRCS = server/src/main.c server/src/netcomm.c server/src/server.c
HDRS = server/inc/netcomm.h server/inc/server.h

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
EXEC = bin/my_server

# Default rule
all: $(EXEC)

# Link object files to create executable
$(EXEC): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $^

# Clean rule
clean:
	rm -f $(EXEC) $(OBJS)

.PHONY: all clean
