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

install: all LICENSE
	install -Ds -m 755 powerbuttond $(DESTDIR)/usr/lib/hwsupport/powerbuttond
	install -D -m 644 LICENSE $(DESTDIR)/usr/share/licenses/powerbuttond
	install -D -m 644 powerbuttond@.service $(DESTDIR)/usr/lib/systemd/user/powerbuttond@.service
