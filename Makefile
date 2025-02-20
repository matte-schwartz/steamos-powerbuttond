CFLAGS += $(shell pkg-config --cflags libevdev libudev) -Wall -Wextra -O2
LDFLAGS += $(shell pkg-config --libs libevdev libudev) -O2

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
	install -D -m 644 steamos-powerbuttond.service $(DESTDIR)/usr/lib/systemd/user/steamos-powerbuttond.service
	install -D -m 644 steamos-power-button.rules $(DESTDIR)/usr/lib/udev/rules.d/80-steamos-power-button.rules
	install -D -m 644 steamos-power-button.hwdb $(DESTDIR)/usr/lib/udev/hwdb.d/80-steamos-power-button.hwdb
