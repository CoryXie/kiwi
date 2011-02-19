/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		String to double function.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/** Convert a string to a double precision number.
 *
 * Converts a string to a double-precision number.
 *
 * @param s		String to convert.
 * @param endptr	Pointer to store end of string in (can be NULL).
 *
 * @return		Converted number.
 */
double strtod(const char *restrict s, char **restrict endptr) {
	const char *p = s;
	long double factor, value = 0.L;
	int sign = 1;
	unsigned int expo = 0;

	while(isspace(*p)) {
		p++;
	}

	switch(*p) {
	case '-':
		sign = -1;
	case '+':
		p++;
	default:
		break;
	}

	while((unsigned int)(*p - '0') < 10u) {
		value = value * 10 + (*p++ - '0');
	}

	if(*p == '.') {
		factor = 1.;
		p++;

		while((unsigned int)(*p - '0') < 10u) {
			factor *= 0.1;
			value += (*p++ - '0') * factor;
		}
	}

	if((*p | 32) == 'e') {
		factor = 10.L;

		switch (*++p) {
		case '-':
			factor = 0.1;
		case '+':
			p++;
			break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			break;
		default:
			value = 0.L;
			p = s;
			goto done;
		}

		while((unsigned int)(*p - '0') < 10u) {
			expo = 10 * expo + (*p++ - '0');
		}

		while(1) {
			if(expo & 1) {
				value *= factor;
			}
			if((expo >>= 1) == 0) {
				break;
			}
			factor *= factor;
		}
	}
done:
	if(endptr != NULL) {
		*endptr = (char *)p;
	}

	return value * sign;
}
