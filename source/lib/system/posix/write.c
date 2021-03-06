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
 * @brief               POSIX write functions.
 *
 * TODO:
 *  - When a failure occurs after partially writing the data the kernel updates
 *    the handle's offset by the number of bytes that were successfully written.
 *    This is possibly incorrect for POSIX.
 */

#include <kernel/device.h>
#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "libsystem.h"

/**
 * Write to a particular position in a file.
 *
 * Writes to the specified position in a file. The file descriptor's current
 * offset will be ignored, and will not be updated after the write.
 *
 * @param fd            File descriptor to write to.
 * @param buf           Buffer containing data to write.
 * @param count         Number of bytes to write.
 * @param offset        Offset into the file to write to.
 *
 * @return              Number of bytes written on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    size_t bytes;
    status_t ret;

    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }

    ret = kern_file_write(fd, buf, count, offset, &bytes);
    if (ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
        if (ret == STATUS_ACCESS_DENIED) {
            errno = EBADF;
        } else {
            libsystem_status_to_errno(ret);
        }

        return -1;
    }

    return (ssize_t)bytes;
}

/**
 * Write to a file.
 *
 * Writes to a file. After the write, the file descriptor's offset will be
 * updated by the number of bytes written.
 *
 * @param fd            File descriptor to write to.
 * @param buf           Buffer containing data to write.
 * @param count         Number of bytes to write.
 *
 * @return              Number of bytes written on success, -1 on failure (errno
 *                      will be set appropriately).
 */
ssize_t write(int fd, const void *buf, size_t count) {
    size_t bytes;
    status_t ret;

    ret = kern_file_write(fd, buf, count, -1, &bytes);
    if (ret != STATUS_SUCCESS && (ret != STATUS_INTERRUPTED || bytes == 0)) {
        if (ret == STATUS_ACCESS_DENIED) {
            errno = EBADF;
        } else {
            libsystem_status_to_errno(ret);
        }

        return -1;
    }

    return (ssize_t)bytes;
}
