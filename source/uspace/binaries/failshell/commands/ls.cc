/* Kiwi shell - Directory list command
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Directory list command.
 */

#include <kernel/errors.h>
#include <kernel/fs.h>
#include <kernel/handle.h>

#include <stdio.h>
#include <stdlib.h>

#include "../failshell.h"

/** Directory list command. */
class LSCommand : Shell::Command {
public:
	LSCommand() : Command("ls", "Show the contents of a directory.") {}

	/** Change the current directory.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		fs_dir_entry_t *entry;
		char path[4096];
		const char *dir;
		handle_t handle;
		fs_info_t info;
		int ret;

		if(SHELL_HELP(argc, argv) || (argc != 1 && argc != 2)) {
			printf("Usage: %s [<directory>]\n", argv[0]);
			return -ERR_PARAM_INVAL;
		}

		dir = (argc == 2) ? argv[1] : ".";
		if((handle = fs_dir_open(dir, 0)) < 0) {
			printf("Failed to open directory (%d)\n", handle);
			return handle;
		} else if(!(entry = reinterpret_cast<fs_dir_entry_t *>(malloc(4096)))) {
			printf("Failed to allocate directory entry\n");
			return -ERR_NO_MEMORY;
		}

		printf("ID   Links  Size       Name\n");
		printf("==   =====  ====       ====\n");

		while(true) {
			if((ret = fs_dir_read(handle, entry, 4096, -1)) != 0) {
				handle_close(handle);
				free(entry);

				if(ret != -ERR_NOT_FOUND) {
					printf("Failed to read directory entry (%d)\n", ret);
					return ret;
				}
				return 0;
			}

			strcpy(path, dir);
			strcat(path, "/");
			strcpy(path, entry->name);

			/* Get information. */
			if((ret = fs_info(path, false, &info)) != 0) {
				printf("Failed to get entry information (%d)\n", ret);
				handle_close(handle);
				free(entry);
				return ret;
			}

			printf("%-4d %-6zu %-10llu ", info.id, info.links, info.size);
			if((ret = fs_symlink_read(path, path, 4096)) > 0) {
				printf("%s -> %s\n", entry->name, path);
			} else {
				printf("%s\n", entry->name);
			}
		}
	}
};

/** Instance of the LS command. */
static LSCommand ls_command;
