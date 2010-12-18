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
 * @brief		Terminal window class.
 */

#include <kiwi/Graphics/Surface.h>

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <pixman.h>
#include <stdlib.h>

#include "TerminalWindow.h"

using namespace kiwi;
using namespace std;

/** Normal terminal font. */
Font *TerminalWindow::m_font = 0;

/** Bold terminal font. */
Font *TerminalWindow::m_bold_font = 0;

/** Colour conversion table. */
static struct { double r; double g; double b; } colour_table[2][9] = {
	{
		{ 0.0, 0.0, 0.0 },	/**< kBlackColour. */
		{ 0.7, 0.1, 0.1 },	/**< kRedColour. */
		{ 0.1, 0.7, 0.1 },	/**< kGreenColour. */
		{ 0.7, 0.4, 0.1 },	/**< kYellowColour. */
		{ 0.1, 0.1, 0.1 },	/**< kBlueColour */
		{ 0.7, 0.1, 0.7 },	/**< kMagentaColour. */
		{ 0.1, 0.7, 0.7 },	/**< kCyanColour. */
		{ 0.7, 0.7, 0.7 },	/**< kWhiteColour. */
		{ 0.0, 0.0, 0.0 },	/**< kDefaultColour (ignored). */
	},
	{
		{ 0.4, 0.4, 0.4 },	/**< kBlackColour. */
		{ 1.0, 0.3, 0.3 },	/**< kRedColour. */
		{ 0.3, 1.0, 0.3 },	/**< kGreenColour. */
		{ 1.0, 1.0, 0.3 },	/**< kYellowColour. */
		{ 0.3, 0.3, 1.0 },	/**< kBlueColour */
		{ 1.0, 0.3, 1.0 },	/**< kMagentaColour. */
		{ 0.3, 1.0, 1.0 },	/**< kCyanColour. */
		{ 1.0, 1.0, 1.0 },	/**< kWhiteColour. */
		{ 0.0, 0.0, 0.0 },	/**< kDefaultColour (ignored). */
	},
};

/** Create a new terminal window.
 * @param app		Application the window is for.
 * @param cols		Intial number of columns.
 * @param rows		Initial number of rows. */
TerminalWindow::TerminalWindow(TerminalApp *app, int cols, int rows) :
	m_app(app), m_xterm(this), m_terminal(&m_xterm, cols, rows),
	m_cols(cols), m_rows(rows), m_history_pos(0)
{
	int id;

	m_terminal.OnExit.Connect(this, &TerminalWindow::TerminalExited);

	/* Create the font if necessary. */
	if(!m_font) {
		m_font = new Font("/system/data/fonts/DejaVuSansMono.ttf", 13.0);
		m_bold_font = new Font("/system/data/fonts/DejaVuSansMono-Bold.ttf", 13.0);
	}

	/* Work out the size to give the window. */
	Size size = m_font->GetSize();
	size = Size(size.GetWidth() * cols, size.GetHeight() * rows);
	Resize(size);

	/* Set up the window. The resize event generated by the Resize() call
	 * will draw the window for us. */
	SetTitle("Terminal");
	id = (m_terminal.GetID() % 4) + 1;
	MoveTo(Point(id * 50, id * 75));

	/* Show the window. */
	Show();
}

/** Update an area in the terminal buffer.
 * @param rect		Area to update. */
void TerminalWindow::TerminalUpdated(Rect rect) {
	cairo_t *context;
	int csr_x, csr_y;

	m_terminal.GetBuffer()->GetCursor(csr_x, csr_y);
	Size font_size = m_font->GetSize();

	/* Create the context. */
	context = cairo_create(GetSurface()->GetCairoSurface());

	/* Draw the characters. */
	for(int y = rect.GetY(); y < (rect.GetY() + rect.GetHeight()); y++) {
		/* Ignore rows that we are not currently looking at. */
		if(y >= (m_history_pos + m_rows)) {
			continue;
		}

		for(int x = rect.GetX(); x < (rect.GetX() + rect.GetWidth()); x++) {
			bool invert;

			/* Get the character. If it is the cursor, invert colours. */
			TerminalBuffer::Character ch = m_terminal.GetBuffer()->CharAt(x, y);
			if((invert = (x == csr_x && y == csr_y))) {
				swap(ch.fg, ch.bg);
			}

			/* Work out where to draw at. */
			Point pos(x * font_size.GetWidth(), (y - m_history_pos) * font_size.GetHeight());

			/* Select the background colour. */
			if(ch.bg != TerminalBuffer::kDefaultColour) {
				cairo_set_source_rgb(
					context,
					colour_table[0][ch.bg].r,
					colour_table[0][ch.bg].g,
					colour_table[0][ch.bg].b
				);
			} else {
				if(invert) {
					cairo_set_source_rgb(context, 1, 1, 1);
				} else {
					cairo_set_source_rgba(context, 0, 0, 0, 0.9);
				}
			}

			/* Draw the background. */
			cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
			cairo_rectangle(context, pos.GetX(), pos.GetY(), font_size.GetWidth(), font_size.GetHeight());
			cairo_fill(context);

			/* Select the foreground colour. */
			if(ch.fg != TerminalBuffer::kDefaultColour) {
				cairo_set_source_rgb(
					context,
					colour_table[ch.bold][ch.fg].r,
					colour_table[ch.bold][ch.fg].g,
					colour_table[ch.bold][ch.fg].b
				);
			} else {
				if(invert) {
					cairo_set_source_rgb(context, 0, 0, 0);
				} else {
					cairo_set_source_rgb(context, 1, 1, 1);
				}
			}

			/* Draw the character. */
			cairo_set_operator(context, CAIRO_OPERATOR_OVER);
			if(ch.bold) {
				m_bold_font->DrawChar(context, ch.ch, pos);
			} else {
				m_font->DrawChar(context, ch.ch, pos);
			}
			m_updated.Union(Rect(pos, font_size));
		}
	}

	cairo_destroy(context);
}

/** Scroll part of the main area of the terminal.
 * @param start		Start of scroll region.
 * @param end		End of scroll region (inclusive).
 * @param delta		Position delta (-1 for scroll down, 1 for scroll up). */
void TerminalWindow::TerminalScrolled(int start, int end, int delta) {
	assert(start >= 0 && start < m_rows);
	assert(end >= start && end < m_rows);

	start -= m_history_pos;
	if(start >= m_rows) {
		return;
	}

	end -= m_history_pos;
	if(end >= m_rows) {
		end = m_rows - 1;
	}

	DoScroll(start, end, delta);
}

/** Handle a line being added to the history. */
void TerminalWindow::TerminalHistoryAdded() {
	if(m_history_pos == 0) {
		TerminalScrolled(0, m_rows - 1, -1);
	} else if(abs(m_history_pos) == m_terminal.GetBuffer()->GetHistorySize()) {
		m_history_pos--;
		ScrollDown(1);
	} else {
		m_history_pos--;
	}
}

/** Handle the terminal buffer being changed. */
void TerminalWindow::TerminalBufferChanged() {
	m_history_pos = 0;
	TerminalUpdated(Rect(0, 0, m_cols, m_rows));
}

/** Flush updates to the window. */
void TerminalWindow::Flush() {
	/* Update the redrawn area. */
	Region::RectArray rects;
	m_updated.GetRects(rects);
	for(auto it = rects.begin(); it != rects.end(); ++it) {
		Update(*it);
	}

	m_updated.Clear();
}

/** Handle the terminal process exiting.
 * @param status	Exit status of the process. */
void TerminalWindow::TerminalExited(int status) {
	DeleteLater();
}

/** Move up in the history.
 * @param amount	Number of rows to scroll up. */
void TerminalWindow::ScrollUp(int amount) {
	int npos = std::max(-static_cast<int>(m_terminal.GetBuffer()->GetHistorySize()), m_history_pos - amount);
	if(npos < m_history_pos) {
		int delta = m_history_pos - npos;
		m_history_pos = npos;
		if(delta == 1) {
			DoScroll(0, m_rows - 1, 1);
		} else {
			TerminalUpdated(Rect(0, m_history_pos, m_cols, m_rows));
		}
	}
}

/** Move down in the history.
 * @param amount	Number of rows to scroll down. */
void TerminalWindow::ScrollDown(int amount) {
	int npos = std::min(0, m_history_pos + amount);
	if(npos > m_history_pos) {
		int delta = npos - m_history_pos;
		m_history_pos = npos;
		if(delta == 1) {
			DoScroll(0, m_rows - 1, -1);
		} else {
			TerminalUpdated(Rect(0, m_history_pos, m_cols, m_rows));
		}
	}
}

/** Scroll part of the visible area.
 * @param start		Start of scroll region in visible area.
 * @param end		End of scroll region in visible area (inclusive).
 * @param delta		Position delta (-1 for scroll down, 1 for scroll up). */
void TerminalWindow::DoScroll(int start, int end, int delta) {
	int csr_x, csr_y;

	assert(start >= 0 && start < m_rows);
	assert(end >= start && end < m_rows);

	/* Only need to copy if the scroll region is larger than 1 line. */
	if(end > start) {
		uint32_t *data = GetSurface()->GetData();
		int src_y, dest_y;

		/* Work out where to draw from and to. */
		Size font_size = m_font->GetSize();
		if(delta < 0) {
			src_y = font_size.GetHeight() * (start + 1);
			dest_y = font_size.GetHeight() * start;
		} else {
			src_y = font_size.GetHeight() * start;
			dest_y = font_size.GetHeight() * (start + 1);
		}

		/* Work out the size of the area to draw. */
		Size size(GetFrame().GetWidth(), font_size.GetHeight() * (end - start));

		/* Scroll the area. Unfortunately pixman does not properly
		 * support overlapping blits, it will corrupt the surface (the
		 * same occurs with Cairo, which uses pixman. */
		if(delta < 0) {
			pixman_blt(data, data, size.GetWidth(), size.GetWidth(),
			           32, 32, 0, src_y, 0, dest_y, size.GetWidth(),
			           size.GetHeight());
		} else {
			memmove(data + (dest_y * size.GetWidth()),
			        data + (src_y * size.GetWidth()),
			        size.GetWidth() * size.GetHeight() * 4);
		}

		/* Update the window. */
		m_updated.Union(Rect(Point(0, dest_y), size));
	}

	/* Update characters on the top/bottom row. */
	TerminalUpdated(Rect(0, m_history_pos + ((delta > 0) ? start : end), m_cols, 1));

	/* If the cursor was in the scrolled area, redraw the old position. */
	m_terminal.GetBuffer()->GetCursor(csr_x, csr_y);
	if(csr_y >= start && csr_y <= end) {
		TerminalUpdated(Rect(csr_x, csr_y + delta, m_cols, 1));
	}
}

/** Send input to the terminal.
 * @param ch		Character to send. */
void TerminalWindow::SendInput(unsigned char ch) {
	/* The purpose of this function is to move back to the main area of
	 * the terminal if we're currently looking at history. The reason it's
	 * not done in KeyPressed() below is that not all key presses will
	 * result in input being sent. */
	if(m_history_pos < 0) {
		ScrollDown(-m_history_pos);
	}

	m_terminal.Input(ch);
}

/** Handle a key press event on the window.
 * @param event		Event information structure. */
void TerminalWindow::KeyPressed(const KeyEvent &event) {
	uint32_t modifiers = event.GetModifiers() & (Input::kControlModifier | Input::kShiftModifier);
	if(modifiers == (Input::kControlModifier | Input::kShiftModifier)) {
		/* Handle keyboard shortcuts. */
		switch(event.GetKey()) {
		case INPUT_KEY_N:
			m_app->CreateWindow();
			break;
		case INPUT_KEY_UP:
			ScrollUp(1);
			break;
		case INPUT_KEY_DOWN:
			ScrollDown(1);
			break;
		case INPUT_KEY_PGUP:
			ScrollUp(m_rows);
			break;
		case INPUT_KEY_PGDOWN:
			ScrollDown(m_rows);
			break;
		}
	} else {
		/* Send the key to the terminal. */
		switch(event.GetKey()) {
		case INPUT_KEY_INSERT:
			SendInput(0x1B);
			SendInput('[');
			SendInput('2');
			SendInput('~');
			break;
		case INPUT_KEY_HOME:
			SendInput(0x1B);
			SendInput('[');
			SendInput('H');
			break;
		case INPUT_KEY_PGUP:
			SendInput(0x1B);
			SendInput('[');
			SendInput('5');
			SendInput('~');
			break;
		case INPUT_KEY_PGDOWN:
			SendInput(0x1B);
			SendInput('[');
			SendInput('6');
			SendInput('~');
			break;
		case INPUT_KEY_END:
			SendInput(0x1B);
			SendInput('[');
			SendInput('F');
			break;
		case INPUT_KEY_DELETE:
			SendInput(0x1B);
			SendInput('[');
			SendInput('3');
			SendInput('~');
			break;
		case INPUT_KEY_UP:
			SendInput(0x1B);
			SendInput('[');
			SendInput('A');
			break;
		case INPUT_KEY_DOWN:
			SendInput(0x1B);
			SendInput('[');
			SendInput('B');
			break;
		case INPUT_KEY_LEFT:
			SendInput(0x1B);
			SendInput('[');
			SendInput('D');
			break;
		case INPUT_KEY_RIGHT:
			SendInput(0x1B);
			SendInput('[');
			SendInput('C');
			break;
		case INPUT_KEY_F1:
			SendInput(0x1B);
			SendInput('O');
			SendInput('P');
			break;
		case INPUT_KEY_F2:
			SendInput(0x1B);
			SendInput('O');
			SendInput('Q');
			break;
		case INPUT_KEY_F3:
			SendInput(0x1B);
			SendInput('O');
			SendInput('R');
			break;
		case INPUT_KEY_F4:
			SendInput(0x1B);
			SendInput('O');
			SendInput('S');
			break;
		case INPUT_KEY_F5:
			SendInput(0x1B);
			SendInput('[');
			SendInput('1');
			SendInput('5');
			SendInput('~');
			break;
		case INPUT_KEY_F6:
			SendInput(0x1B);
			SendInput('[');
			SendInput('1');
			SendInput('7');
			SendInput('~');
			break;
		case INPUT_KEY_F7:
			SendInput(0x1B);
			SendInput('[');
			SendInput('1');
			SendInput('8');
			SendInput('~');
			break;
		case INPUT_KEY_F8:
			SendInput(0x1B);
			SendInput('[');
			SendInput('1');
			SendInput('9');
			SendInput('~');
			break;
		case INPUT_KEY_F9:
			SendInput(0x1B);
			SendInput('[');
			SendInput('2');
			SendInput('0');
			SendInput('~');
			break;
		case INPUT_KEY_F10:
			SendInput(0x1B);
			SendInput('[');
			SendInput('2');
			SendInput('1');
			SendInput('~');
			break;
		case INPUT_KEY_F11:
			SendInput(0x1B);
			SendInput('[');
			SendInput('2');
			SendInput('3');
			SendInput('~');
			break;
		case INPUT_KEY_F12:
			SendInput(0x1B);
			SendInput('[');
			SendInput('2');
			SendInput('4');
			SendInput('~');
			break;
		default:
			string text = event.GetText();
			for(auto it = text.begin(); it != text.end(); ++it) {
				SendInput(*it);
			}
			break;
		}
	}
}

/** Handle the window being resized.
 * @param event		Event information structure. */
void TerminalWindow::Resized(const ResizeEvent &event) {
	cairo_t *context;
	int cols, rows;

	/* Compute the new number of columns/rows. */
	Size font_size = m_font->GetSize();
	cols = event.GetSize().GetWidth() / font_size.GetWidth();
	rows = event.GetSize().GetHeight() / font_size.GetHeight();
	m_terminal.Resize(cols, rows);

	/* Create the context to re-render. */
	context = cairo_create(GetSurface()->GetCairoSurface());

	/* Initialise the background. */
	cairo_rectangle(context, 0, 0, event.GetSize().GetWidth(), event.GetSize().GetHeight());
	cairo_set_source_rgba(context, 0, 0, 0, 0.9);
	cairo_fill(context);

	/* Destroy the context and update the window. */
	cairo_destroy(context);
	m_updated.Union(GetFrame());
}
