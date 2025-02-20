// SPDX-License-Identifier: BSD-2-Clause
//
// Copyright (c) 2023-2025 Valve Software
// Maintainer: Vicki Pfau <vi@endrift.com>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <libudev.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

void signal_handler(int) {
}

struct libevdev* open_dev(const char* path) {
	int fd = open(path, O_RDONLY);
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

struct libevdev* find_dev(void) {
	struct udev* udev = udev_new();
	char* path = NULL;
	if (!udev) {
		return NULL;
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

	 for (; devices && !path; devices = udev_list_entry_get_next(devices)) {
		const char* syspath = udev_list_entry_get_name(devices);
		struct udev_device* dev = udev_device_new_from_syspath(udev, syspath);
		if (!dev) {
			continue;
		}
		const char* devpath = udev_device_get_devnode(dev);
		if (devpath) {
			path = strdup(devpath);
		}
		udev_device_unref(dev);
	 }

out:
	if (uenum) {
		udev_enumerate_unref(uenum);
	}
	udev_unref(udev);

	if (path == NULL) {
		return NULL;
	}
	struct libevdev* dev = open_dev(path);
	free(path);
	return dev;
}

void do_press(const char* type) {
	char steam[PATH_MAX];
	char press[32];
	char* home = getenv("HOME");
	char* const args[] = {steam, "-ifrunning", press, NULL};

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

	struct libevdev* dev;
	if (argc >= 2) {
		dev = open_dev(argv[1]);
	} else {
		dev = find_dev();
	}
	if (!dev) {
		return 0;
	}

	bool press_active = false;
	while (true) {
		struct input_event ev;
		int res = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_BLOCKING, &ev);
		if (res == LIBEVDEV_READ_STATUS_SUCCESS) {
			if (ev.type == EV_KEY && ev.code == KEY_POWER) {
				if (ev.value == 1) {
					press_active = true;
					alarm(1);
				} else if (press_active) {
					press_active = false;
					alarm(0);
					do_press("short");
				}
			}
		} else if (res == -EINTR && press_active) {
			press_active = false;
			alarm(0);
			do_press("long");
		}
	}
}
