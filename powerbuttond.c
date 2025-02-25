// SPDX-License-Identifier: BSD-2-Clause
//
// Copyright (c) 2023-2025 Valve Software
// Maintainer: Vicki Pfau <vi@endrift.com>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <libudev.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_DEVS 2

extern char** environ;

void signal_handler(int) {
}

struct libevdev* open_dev(const char* path) {
	int fd = open(path, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return NULL;
	}
	struct libevdev* dev;
	if (libevdev_new_from_fd(fd, &dev) < 0) {
		close(fd);
		return NULL;
	}
	if (!libevdev_has_event_code(dev, EV_KEY, KEY_POWER)) {
		libevdev_free(dev);
		return NULL;
	}
	return dev;
}

size_t find_devs(struct libevdev* devs[]) {
	struct udev* udev = udev_new();
	size_t num_devs = 0;
	if (!udev) {
		return 0;
	}
	struct udev_enumerate* uenum = udev_enumerate_new(udev);
	if (!uenum) {
		goto out;
	}

	if (udev_enumerate_add_match_subsystem(uenum, "input") < 0) {
		goto out;
	}

	if (udev_enumerate_add_match_sysname(uenum, "event*") < 0) {
		goto out;
	}

	if (udev_enumerate_add_match_property(uenum, "STEAMOS_POWER_BUTTON", "1") < 0) {
		goto out;
	}

	if (udev_enumerate_scan_devices(uenum) < 0) {
		goto out;
	}

	struct udev_list_entry* devices = udev_enumerate_get_list_entry(uenum);

	for (; devices && num_devs < MAX_DEVS; devices = udev_list_entry_get_next(devices)) {
		const char* syspath = udev_list_entry_get_name(devices);
		struct udev_device* dev = udev_device_new_from_syspath(udev, syspath);
		if (!dev) {
			continue;
		}
		const char* devpath = udev_device_get_devnode(dev);
		if (devpath) {
			struct libevdev* evdev = open_dev(devpath);
			if (evdev) {
				printf("Found power button device at %s\n", devpath);
				devs[num_devs] = evdev;
				++num_devs;
			}
		}
		udev_device_unref(dev);
	}

out:
	if (uenum) {
		udev_enumerate_unref(uenum);
	}
	udev_unref(udev);

	return num_devs;
}

void do_press(const char* type) {
	char steam[PATH_MAX];
	char press[32];
	char* home = getenv("HOME");
	char* const args[] = {steam, "-ifrunning", press, NULL};

	alarm(0);

	snprintf(steam, sizeof(steam), "%s/.steam/root/ubuntu12_32/steam", home);
	snprintf(press, sizeof(press), "steam://%spowerpress", type);

	pid_t pid;
	if (posix_spawn(&pid, steam, NULL, NULL, args, environ) < 0) {
		return;
	}
	while (true) {
		if (waitpid(pid, NULL, 0) > 0) {
			break;
		}
		if (errno != EINTR && errno != EAGAIN) {
			break;
		}
	}
}

int main(int argc, char* argv[]) {
	struct sigaction sa = {
		.sa_handler = signal_handler,
		.sa_flags = SA_NOCLDSTOP,
	};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);

	struct libevdev* devs[MAX_DEVS] = {0};
	struct pollfd pfds[MAX_DEVS] = {0};
	size_t num_devs = 0;

	if (argc >= 2) {
		int i;
		for (i = 0; i < argc - 1; ++i) {
			devs[num_devs] = open_dev(argv[i + 1]);
			if (devs[num_devs]) {
				++num_devs;
			}
		}
	} else {
		num_devs = find_devs(devs);
	}
	if (!num_devs) {
		return 0;
	}
	size_t i;
	for (i = 0; i < num_devs; ++i) {
		pfds[i].fd = libevdev_get_fd(devs[i]);
		pfds[i].events = POLLIN;
	}

	bool press_active = false;
	while (true) {
		for (i = 0; i < num_devs; ++i) {
			pfds[i].fd = libevdev_get_fd(devs[i]);
			pfds[i].revents = 0;
		}

		int res = poll(pfds, num_devs, -1);
		if (res < 0 && errno == EINTR && press_active) {
			press_active = false;
			do_press("long");
		} else if (res <= 0) {
			continue;
		}

		for (i = 0; i < num_devs; ++i) {
			if (!(pfds[i].revents & POLLIN)) {
				continue;
			}
			struct input_event ev;
			do {
				res = libevdev_next_event(devs[i], LIBEVDEV_READ_FLAG_NORMAL, &ev);
				while (res == LIBEVDEV_READ_STATUS_SYNC) {
					res = libevdev_next_event(devs[i], LIBEVDEV_READ_FLAG_SYNC, &ev);
				}
				if (res != LIBEVDEV_READ_STATUS_SUCCESS) {
					break;
				}
				if (ev.type == EV_KEY) {
					if (ev.code == KEY_POWER) {
						if (ev.value == 1) {
							press_active = true;
							alarm(1);
						} else if (press_active) {
							do_press("short");
						}
					} else if (ev.code == KEY_LEFTMETA && ev.value == 1) {
						press_active = false;
						do_press("long");
					}
				}
			} while (libevdev_has_event_pending(devs[i]) > 0);
			if (res == -EINTR && press_active) {
				press_active = false;
				do_press("long");
			}
		}
	}
}
