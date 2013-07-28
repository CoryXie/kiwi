/*
 * Copyright (C) 2008-2013 Alex Smith
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
 * @brief		Character type functions.
 */

#include <ctype.h>

/** Test if a character is alpha-numeric.
 * @param ch		Character to test.
 * @return		Non-zero if is alpha-numeric, zero if not. */
int isalnum(int ch) {
	return (isalpha(ch) || isdigit(ch));
}

/** Test if a character is alphabetic.
 * @param ch		Character to test.
 * @return		Non-zero if is alphabetic, zero if not. */
int isalpha(int ch) {
	return (islower(ch) || isupper(ch));
}

/** Test if a character is an ASCII character.
 * @param ch		Character to test.
 * @return		Non-zero if is ASCII, zero if not. */
int isascii(int ch) {
	return ((unsigned int)ch < 128u);
}

/** Test if a character is blank.
 * @param ch		Character to test.
 * @return		Non-zero if is blank space, zero if not. */
int isblank(int ch) {
	return (ch == ' ' || ch == '\t');
}

/** Test if a character is a control character.
 * @param ch		Character to test.
 * @return		Non-zero if is control character, zero if not. */
int iscntrl(int ch) {
	return ((unsigned int)ch < 32u || ch == 127);
}

/** Test if a character is a digit.
 * @param ch		Character to test.
 * @return		Non-zero if is digit, zero if not. */
int isdigit(int ch) {
	return (ch >= '0' && ch <= '9');
}

/** Check for any printable character except space.
 * @param ch		Character to test.
 * @return		Non-zero if check passed, zero if not. */
int isgraph(int ch) {
	if(ch == ' ')
		return 0;

	return isprint(ch);
}

/** Test if a character is lower-case.
 * @param ch		Character to test.
 * @return		Non-zero if is lower-case, zero if not. */
int islower(int ch) {
	return (ch >= 'a' && ch <= 'z');
}

/** Test if a character is a printable character.
 * @param ch		Character to test.
 * @return		Non-zero if is printable, zero if not. */
int isprint(int ch) {
	ch &= 0x7F;
	return (ch >= 0x20 && ch < 0x7F);
}

/** Test if a character is punctuation.
 * @param ch		Character to test.
 * @return		Non-zero if is punctuation, zero if not. */
int ispunct(int ch) {
	return (isprint(ch) && !isalnum(ch) && !isspace(ch));
}

/** Test if a character is whitespace.
 * @param ch		Character to test.
 * @return		Non-zero if is whitespace, zero if not. */
int isspace(int ch) {
	return (ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ');
}

/** Test if a character is upper-case.
 * @param ch		Character to test.
 * @return		Non-zero if is upper-case, zero if not. */
int isupper(int ch) {
	return (ch >= 'A' && ch <= 'Z');
}

/** Test if a character is a hexadecimal digit.
 * @param ch		Character to test.
 * @return		Non-zero if is alphabetic, zero if not. */
int isxdigit(int ch) {
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/**
 * Convert a character to ASCII.
 *
 * Converts a character to a 7-bit value that fits into the ASCII character
 * set. Using this function will upset people, as it converts accented
 * characters into random characters.
 *
 * @param ch		Character to convert.
 *
 * @return		Converted value.
 */
int toascii(int ch) {
	return (ch & 0x7F);
}

/** Convert a character to lower-case.
 * @param ch		Character to convert.
 * @return		Converted character. */
int tolower(int ch) {
	if(isalpha(ch)) {
		return ch | 0x20;
	} else {
		return ch;
	}
}

/** Convert a character to upper-case.
 * @param ch		Character to convert.
 * @return		Converted character. */
int toupper(int ch) {
	if(isalpha(ch)) {
		return ch & ~0x20;
	} else {
		return ch;
	}
}