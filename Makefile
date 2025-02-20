LIB_CFLAGS := $(shell pkg-config --cflags libevdev libudev)
LIB_LDFLAGS := $(shell pkg-config --libs libevdev libudev)

CFLAGS += -Wall -Wextra $(LIB_CFLAGS)

ifneq ($(ASAN),)
  CFLAGS += -g -fsanitize=address
  LDFLAGS += -g -fsanitize=address
else ifneq ($(DEBUG),)
  CFLAGS += -g
  LDFLAGS += -g
else
  CFLAGS += -O2
  LDFLAGS += -O2
endif

all: powerbuttond
.PHONY: clean install

powerbuttond: powerbuttond.o
	$(CC) $(LIB_LDFLAGS) $(LDFLAGS) $< -o $@

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
