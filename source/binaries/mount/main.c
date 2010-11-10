/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Mount command.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** Print a usage message. */
static void usage(const char *argv0) {
	printf("Usage: %s [<dev> <path> <type> [<opts>]]\n", argv0);
}

/** List information on all mounts. */
static int mount_list(const char *argv0) {
	fs_mount_info_t *mounts;
	size_t count, i;
	status_t ret;

	ret = fs_mount_info(NULL, &count);
	if(ret != STATUS_SUCCESS) {
		printf("%s: %s\n", argv0, __kernel_status_strings[ret]);
		return EXIT_FAILURE;
	}

	mounts = malloc(count * sizeof(*mounts));
	if(!mounts) {
		perror(argv0);
		return EXIT_FAILURE;
	}

	ret = fs_mount_info(mounts, &count);
	if(ret != STATUS_SUCCESS) {
		printf("%s: %s\n", argv0, __kernel_status_strings[ret]);
		return EXIT_FAILURE;
	}

	for(i = 0; i < count; i++) {
		if(strlen(mounts[i].device)) {
			printf("%s:%s on %s\n", mounts[i].type, mounts[i].device, mounts[i].path);
		} else {
			printf("%s on %s\n", mounts[i].type, mounts[i].path);
		}
	}

	return EXIT_SUCCESS;
}

/** Main function for the mount command. */
int main(int argc, char **argv) {
	const char *dev, *path, *type, *opts;
	status_t ret;

	if(argc >= 2 && strcmp(argv[1], "--help") == 0) {
		usage(argv[0]);
		return EXIT_SUCCESS;
	}

	if(argc < 2) {
		return mount_list(argv[0]);
	} else if(argc == 4 || argc == 5) {
		dev = argv[1];
		path = argv[2];
		type = argv[3];
		opts = (argc == 5) ? argv[4] : 0;

		ret = fs_mount(dev, path, type, opts);
		if(ret != STATUS_SUCCESS) {
			printf("%s: %s\n", argv[0], __kernel_status_strings[ret]);
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	} else {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
}