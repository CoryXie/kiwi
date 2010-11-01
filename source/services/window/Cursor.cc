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
 * @brief		Cursor class.
 *
 * This class implements the cursor, using a special window type. This window
 * type is kept above all other windows and cannot be made active. The window
 * is not published in the session, so clients are unable to do things to it
 * without going through the cursor calls.
 *
 * @todo		Different cursor types.
 */

#include <cairo/cairo.h>

#include <kiwi/Graphics/Rect.h>

#include <cassert>
#include <exception>
#include <iostream>

#include "Cursor.h"
#include "Session.h"
#include "Surface.h"
#include "Window.h"

using namespace kiwi;
using namespace std;

/** Properties of the cursor. */
static const char *kCursorPath = "/system/data/images/cursor.png";
static const uint16_t kCursorWidth = 24;
static const uint16_t kCursorHeight = 24;
static const int16_t kCursorHotspotX = 6;
static const int16_t kCursorHotspotY = 3;

/** Create the cursor.
 * @param session	Session that the cursor is for. */
Cursor::Cursor(Session *session) :
	m_session(session), m_root(session->GetRoot()), m_grabbed(0)
{
	cairo_surface_t *image;
	cairo_t *context;
	int16_t x, y;

	/* Work out initial placement of the cursor (centre of screen). */
	x = (m_root->GetRect().GetWidth() / 2) - (kCursorWidth / 2);
	y = (m_root->GetRect().GetHeight() / 2) - (kCursorHeight / 2);

	/* Create the cursor window. */
	Rect rect(x, y, kCursorWidth, kCursorHeight);
	m_window = new Window(m_root->GetSession(), -1, m_root, rect, WINDOW_TYPE_CURSOR);

	/* Set up a Cairo context for rendering on to the cursor. */
	context = cairo_create(m_window->GetSurface()->GetCairoSurface());
	if(cairo_status(context) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to create Cairo context: " << cairo_status_to_string(cairo_status(context)) << endl;
		throw exception();
	}

	/* Load the image. */
	image = cairo_image_surface_create_from_png(kCursorPath);
	if(cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		clog << "Failed to load cursor image: ";
		clog << cairo_status_to_string(cairo_surface_status(image)) << endl;
		throw exception();
	}

	/* Check that the cursor is valid. */
	if(cairo_image_surface_get_width(image) != kCursorWidth ||
	   cairo_image_surface_get_height(image) != kCursorHeight) {
		clog << "Warning: Cursor image not correct size" << endl;
	}

	/* Draw the cursor. */
	cairo_set_source_surface(context, image, 0, 0);
	cairo_paint(context);
	cairo_destroy(context);
	cairo_surface_destroy(image);

	/* Make it visible. */
	SetVisible(true);
}

/** Destroy the cursor. */
Cursor::~Cursor() {
	delete m_window;
}

/** Set visibility of the cursor.
 * @param visible	New visibility state. */
void Cursor::SetVisible(bool visible) {
	m_window->SetVisible(visible);
}

/** Move the cursor relative to its current position.
 * @param dx		X delta.
 * @param dy		Y delta. */
void Cursor::MoveRelative(int32_t dx, int32_t dy) {
	int16_t x = m_window->GetRect().GetX() + dx;
	int16_t y = m_window->GetRect().GetY() + dy;

	/* Ensure that the location is within the screen. */
	if(x < -kCursorHotspotX) {
		x = -kCursorHotspotX;
	} else if(x >= (m_root->GetRect().GetWidth() - kCursorHotspotX)) {
		x = (m_root->GetRect().GetWidth() - kCursorHotspotX) - 1;
	}
	if(y < -kCursorHotspotY) {
		y = -kCursorHotspotY;
	} else if(y >= (m_root->GetRect().GetHeight() - kCursorHotspotY)) {
		y = (m_root->GetRect().GetHeight() - kCursorHotspotY) - 1;
	}

	/* Move the window to the new position. */
	dx = x - m_window->GetRect().GetX();
	dy = y - m_window->GetRect().GetY();
	m_window->MoveTo(Point(x, y));
	if(m_grabbed) {
		Point pos = m_grabbed->GetRect().GetTopLeft();
		m_grabbed->MoveTo(Point(pos.GetX() + dx, pos.GetY() + dy));
	}
}

/** Mouse button down handler.
 * @param button	Button that was pressed. */
void Cursor::Down(int32_t button) {
	Window *window;

	/* Get the position the cursor is pointing at. */
	Point pos = m_window->GetAbsoluteRect().GetTopLeft();
	pos = Point(pos.GetX() + kCursorHotspotX, pos.GetY() + kCursorHotspotY);

	/* Get the window at this location. */
	window = m_root->AtPosition(pos);
	assert(window);

	/* Activate the window if it isn't already. */
	m_session->ActivateWindow(window);
	m_grabbed = window;
}

/** Mouse button up handler.
 * @param button	Button that was released. */
void Cursor::Up(int32_t button) {
	m_grabbed = 0;
}
