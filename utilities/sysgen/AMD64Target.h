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
 * @brief		AMD64 system call code generator.
 */

#ifndef __AMD64TARGET_H
#define __AMD64TARGET_H

#include "sysgen.h"

/** AMD64 target class. */
class AMD64Target : public Target {
public:
	void AddTypes(TypeMap &map);
	void Generate(std::ostream &stream, const SyscallList &calls);
};

#endif /* __AMD64TARGET_H */
