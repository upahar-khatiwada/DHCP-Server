CC      = gcc
CFLAGS  = -Wall -Wextra -g
TARGET  = dhcp_server
SRC     = main.c
HDRS    = dhcp.h server.h

all: $(TARGET)

$(TARGET): $(SRC) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	sudo ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean