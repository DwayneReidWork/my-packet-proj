CC = gcc --std gnu99

CFLAGS = -Wall -g

# Source and header files
SRCS = server/src/main.c server/src/netcomm.c server/src/server.c server/src/packet.c server/src/signal.c
HDRS = server/inc/netcomm.h server/inc/server.h server/inc/packet.h server/inc/signal.h

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
EXEC = bin/my_server

# Default rule
all: clean $(EXEC)

# Link object files to create executable
$(EXEC): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $^

# Clean rule
clean:
	rm -f $(EXEC) $(OBJS)

.PHONY: all clean
