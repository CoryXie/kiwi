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
 * @brief               String functions.
 */

#pragma once

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Get size_t and NULL from stddef.h. I would love to know why stdc defines
 * size_t and NULL in 3 headers. */
#define __need_size_t
#include <stddef.h>

#define bzero(b, len)       (memset((b), '\0', (len)), (void)0)
#define bcopy(b1, b2, len)  (memmove((b2), (b1), (len)), (void)0)
#define bcmp(b1, b2, len)   memcmp((b1), (b2), (size_t)(len))
#define index(a, b)         strchr((a),(b))
#define rindex(a, b)        strrchr((a),(b))

#ifdef __cplusplus
}
#endif
