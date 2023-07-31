CFLAGS := $(shell pkg-config --cflags libevdev) -Wall -Wextra -O2
LDFLAGS := $(shell pkg-config --libs libevdev) -O2

all: powerbuttond
.PHONY: clean install

powerbuttond: powerbuttond.o
	$(CC) $(LDFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

clean:
	rm -f powerbuttond powerbuttond.o

install: all
	install -D -m 755 powerbuttond $(DESTDIR)/usr/lib/hwsupport/powerbuttond 
