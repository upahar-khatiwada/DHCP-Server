CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDLIBS  = -lhiredis
TARGET  = dhcp_server
SRC     = main.c
HDRS    = dhcp.h server.h lease.h

all: $(TARGET)

$(TARGET): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

run: $(TARGET)
	sudo ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean