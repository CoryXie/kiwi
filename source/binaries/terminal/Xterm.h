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
 * @brief		Xterm emulator class.
 */

#ifndef __XTERM_H
#define __XTERM_H

#include <string>

#include "Terminal.h"

class TerminalWindow;

/** Class implementing an Xterm emulator. */
class Xterm : public Terminal::Handler {
public:
	Xterm(TerminalWindow *window);
	~Xterm();

	void Resize(int cols, int rows);
	void Output(unsigned char raw);
	TerminalBuffer *GetBuffer();
private:
	TerminalWindow *m_window;	/**< Window that the terminal is on. */
	TerminalBuffer *m_buffers[2];	/**< Main and alternate buffer. */
	int m_active_buffer;		/**< Index of active buffer. */

	int m_esc_state;		/**< Current escape sequence parse state. */
	int m_esc_params[8];		/**< Escape code parameters. */
	int m_esc_param_size;		/**< Number of escape code parameters. */
	std::string m_esc_string;	/**< String escape code parameter. */

	/** Current attributes. */
	TerminalBuffer::Character m_attrib;

	int m_saved_x;			/**< Saved cursor X position. */
	int m_saved_y;			/**< Saved cursor Y position. */
};

#endif /* __XTERM_H */