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
 * @brief		Signal mask function.
 */

#include <kernel/signal.h>
#include <kernel/status.h>

#include <errno.h>
#include <signal.h>

#include "../libc.h"

/** Set the signal mask.
 * @param how		How to set the mask.
 * @param set		Signal set to mask (can be NULL).
 * @param oset		Where to store previous masked signal set (can be NULL).
 * @return		0 on success, -1 on failure. */
int sigprocmask(int how, const sigset_t *restrict set, sigset_t *restrict oset) {
	status_t ret;

	if(how & ~SIGNAL_MASK_ACTION) {
		errno = EINVAL;
		return -1;
	}

	ret = kern_signal_mask(how, set, oset);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
