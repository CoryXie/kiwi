/*
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
 * @brief		Process class.
 */

#ifndef __KIWI_PROCESS_H
#define __KIWI_PROCESS_H

#include <kiwi/Handle.h>

namespace kiwi {

/** Class providing functionality to create and manipulate processes. */
class Process : public Handle {
public:
	Process(handle_id_t handle = -1);

	bool Create(const char *const args[], const char *const env[] = 0, bool usepath = true);
	bool Create(const char *cmdline, const char *const env[] = 0, bool usepath = true);
	bool Open(process_id_t id);

	bool WaitTerminate(useconds_t timeout = -1) const;
	process_id_t GetID(void) const;

	static process_id_t GetCurrentID(void);

	Signal<Process *, int> OnExit;
protected:
	virtual void _EventReceived(int event);
};

}

#endif /* __KIWI_PROCESS_H */
