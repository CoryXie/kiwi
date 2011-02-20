/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		PC kernel debugger functions.
 */

#include <arch/io.h>

#include <pc/console.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <kdbg.h>

/** Keyboard code definitions */
#define	L_CTRL		0x1D		/**< Left control key. */
#define	R_CTRL		0x1D		/**< Right control key. */
#define	L_ALT		0x38		/**< Left alt key. */
#define	R_ALT		0x38		/**< Right alt key. */
#define	L_SHIFT		0x2A		/**< Left shift key. */
#define	R_SHIFT		0x36		/**< Right shift key. */

/** Lower case keyboard layout - United Kingdom */
static const unsigned char kdbg_kbd_layout[] = {
	0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
	'\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, 0, 0,
	'#', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '\\', 0, 0
};

/** Shift keyboard layout - United Kingdom */
static const unsigned char kdbg_kbd_layout_s[] = {
	0, 0, '!', '"', 156, '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
	'\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '@', 0, 0,
	'~', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
	0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0,
	0, 0, '+', 0, 0, 0, 0, 0, 0, 0, '|', 0, 0
};

/** Get a character from the keyboard.
 *
 * Waits for input to become available, takes it out of the i8042 buffer and
 * returns it.
 *
 * @return		Character obtained from keyboard.
 */
unsigned char kdbg_get_char(void) {
	static int shift;
	static int ctrl;
	static int alt;
	unsigned char code, ret;

	while(1) {
#ifdef SERIAL_PORT
		/* Check for serial port data if it is present. */
		code = in8(SERIAL_PORT + 6);
		if((code & ((1<<4) | (1<<5))) && code != 0xFF) {
			if(in8(SERIAL_PORT + 5) & 0x01) {
				code = in8(SERIAL_PORT);

				/* Convert CR to NL, and DEL to Backspace. */
				if(code == '\r') {
					code = '\n';
				} else if(code == 0x7f) {
					code = '\b';
				}
				return code;
			}
		}
#endif

		/* Check for keyboard data. */
		code = in8(0x64);
		if(code & (1<<0) && code & (1<<5)) {
			/* Mouse data, discard. */
			in8(0x60);
			code &= ~(1<<0);
		}
		if((code & (1<<0)) == 0) {
			continue;
		}

		code = in8(0x60);

		if(code & 0x80) {
			code &= 0x7F;

			if(code == L_SHIFT || code == R_SHIFT) {
				shift = 0;
			} else if(code == L_CTRL || code == R_CTRL) {
				ctrl = 0;
			} else if(code == L_ALT || code == R_ALT) {
				alt = 0;
			}
			continue;
		}

		if(code == L_SHIFT || code == R_SHIFT) {
			shift = 1;
		} else if(code == L_CTRL || code == R_CTRL) {
			ctrl = 1;
		} else if(code == L_ALT || code == R_ALT) {
			alt = 1;
		} else {
			if(shift == 1) {
				ret = kdbg_kbd_layout_s[code];
			} else {
				ret = kdbg_kbd_layout[code];
			}

			/* Little hack so that pressing Enter won't result in
			 * an extra newline being sent. */
			if(ret == '\n') {
				while((in8(0x64) & 1) == 0) {};
				in8(0x60);
			}

			if(ret != 0) {
				return ret;
			}
		}
	}
}
