// SPDX-License-Identifier: BSD-2-Clause
//
// Copyright (c) 2023 Valve Software
// Maintainer: Vicki Pfau <vi@endrift.com>
#include <errno.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <limits.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

void signal_handler(int) {
}

struct libevdev* find_dev(const char* path) {
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
	const char* path = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";
	if (argc >= 2) {
		path = argv[1];
	}
	struct sigaction sa = {
		.sa_handler = signal_handler,
		.sa_flags = SA_NOCLDSTOP,
	};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);

	struct libevdev* dev = find_dev(path);
	if (!dev) {
		return 1;
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
