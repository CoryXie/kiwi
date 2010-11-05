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
 * @brief		Compositor class.
 */

#ifndef __COMPOSITOR_H
#define __COMPOSITOR_H

#include <cairo/cairo.h>

#include <kiwi/Graphics/Rect.h>
#include <kiwi/Graphics/Region.h>
#include <kiwi/Support/Noncopyable.h>
#include <kiwi/Timer.h>

#include <list>

#include "ServerWindow.h"

class Display;
class ServerSurface;

/** Class that manages the rendering of windows. */
class Compositor : public kiwi::Object, kiwi::Noncopyable {
public:
	Compositor(Display *display, ServerWindow *root);
	~Compositor();

	void Redraw();
	void Redraw(const kiwi::Rect &rect);
	void Redraw(const kiwi::Region &region);
private:
	void Render(ServerWindow *window, int off_x, int off_y);

	void ScheduleRedraw();
	void PerformRedraw();

	kiwi::Timer m_timer;		/**< Redraw timer. */
	kiwi::Region m_redraw_region;	/**< Redraw region. */
	Display *m_display;		/**< Display to render to. */
	ServerWindow *m_root;		/**< Root window. */
	ServerSurface *m_surface;	/**< Back buffer that rendering takes place on. */
	cairo_t *m_context;		/**< Cairo context for rendering. */
};

#endif /* __COMPOSITOR_H */
