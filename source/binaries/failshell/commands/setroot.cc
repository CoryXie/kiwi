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
 * @brief		Set root directory command.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <iostream>

#include "../failshell.h"

using namespace std;

/** Set root directory command. */
class SetrootCommand : Shell::Command {
public:
	SetrootCommand() : Command("setroot", "Set the root directory.") {}

	/** Set the root directory.
	 * @param argc		Argument count.
	 * @param argv		Argument array.
	 * @return		0 on success, other value on failure. */
	int operator ()(int argc, char **argv) {
		if(SHELL_HELP(argc, argv) || argc != 2) {
			cout << "Usage: " << argv[0] << " <directory>" << endl;
			return STATUS_INVALID_ARG;
		}

		return fs_setroot(argv[1]);
	}
};

/** Instance of the setroot command. */
static SetrootCommand setroot_command;