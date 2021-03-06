/*
 * Copyright (C) 2009-2020 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               POSIX get PID function.
 */

#include "libsystem.h"

#include <kernel/process.h>
#include <kernel/status.h>

#include <unistd.h>

/** Get the current process ID.
 * @return              ID of calling process. */
pid_t getpid(void) {
    process_id_t id;
    status_t ret = kern_process_id(PROCESS_SELF, &id);
    libsystem_assert(ret == STATUS_SUCCESS);
    return id;
}

/** Get the parent process ID.
 * @return              ID of the parent process. */
pid_t getppid(void) {
    /* TODO. */
    return 0;
}
