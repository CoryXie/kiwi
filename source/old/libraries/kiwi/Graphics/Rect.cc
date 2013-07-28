/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Rectangle class.
 */

#include <kiwi/Graphics/Rect.h>
#include <algorithm>

using namespace kiwi;

/** Check whether the rectangle is valid.
 * @return		Whether the rectangle is valid. */
bool Rect::IsValid() const {
	return (GetWidth() > 0 && GetHeight() > 0);
}

/** Check whether a point lies within the rectangle.
 * @param point		Point to check. */
bool Rect::Contains(Point point) const {
	if(point.GetX() >= m_left && point.GetX() < m_right &&
	   point.GetY() >= m_top && point.GetY() < m_bottom) {
		return true;
	} else {
		return false;
	}
}

/** Check whether the rectangle intersects with another.
 * @return rect		Rectangle to check.
 * @return		Whether the rectangles intersect. */
bool Rect::Intersects(Rect rect) const {
	return Intersected(rect).IsValid();
}

/** Intersect the rectangle with another.
 * @param rect		Rectangle to intersect with. */
void Rect::Intersect(Rect rect) {
	m_left = std::max(m_left, rect.m_left);
	m_top = std::max(m_top, rect.m_top);
	m_right = std::min(m_right, rect.m_right);
	m_bottom = std::min(m_bottom, rect.m_bottom);
}

/** Get the area where the rectangle intersects with another.
 * @param rect		Rectangle to intersect with.
 * @return		Area where the rectangles intersect. */
Rect Rect::Intersected(Rect rect) const {
	Point tl(std::max(m_left, rect.m_left), std::max(m_top, rect.m_top));
	Point br(std::min(m_right, rect.m_right), std::min(m_bottom, rect.m_bottom));
	return Rect(tl, br);
}

/** Adjust the rectangle coordinates.
 * @param dx1		Value to add to top left X position.
 * @param dy1		Value to add to top left Y position.
 * @param dx2		Value to add to bottom right X position.
 * @param dy2		Value to add to bottom right Y position. */
void Rect::Adjust(int dx1, int dy1, int dx2, int dy2) {
	m_left += dx1;
	m_top += dy1;
	m_right += dx2;
	m_bottom += dy2;
}

/** Get a new rectangle with adjusted coordinates.
 * @param dx1		Value to add to top left X position.
 * @param dy1		Value to add to top left Y position.
 * @param dx2		Value to add to bottom right X position.
 * @param dy2		Value to add to bottom right Y position.
 * @return		Rectangle with adjustments made. */
Rect Rect::Adjusted(int dx1, int dy1, int dx2, int dy2) const {
	Point tl(m_left + dx1, m_top + dy1);
	Point br(m_right + dx2, m_bottom + dy2);
	return Rect(tl, br);
}

/** Translate the rectangle.
 * @param dx		Value to add to X position.
 * @param dy		Value to add to Y position. */
void Rect::Translate(int dx, int dy) {
	Adjust(dx, dy, dx, dy);
}

/** Get a new translated rectangle.
 * @param dx		Value to add to X position.
 * @param dy		Value to add to Y position.
 * @return		Rectangle with translation applied. */
Rect Rect::Translated(int dx, int dy) const {
	return Adjusted(dx, dy, dx, dy);
}

/** Move the rectangle.
 * @param x		X position to move to.
 * @param y		Y position to move to. */
void Rect::MoveTo(int x, int y) {
	m_right = x + GetWidth();
	m_bottom = y + GetHeight();
	m_left = x;
	m_top = y;
}

/** Move the rectangle.
 * @param pos		New position. */
void Rect::MoveTo(Point pos) {
	MoveTo(pos.GetX(), pos.GetY());
}

/** Set the size of the rectangle.
 * @param width		New width.
 * @param height	New height. */
void Rect::Resize(int width, int height) {
	m_right = GetX() + width;
	m_bottom = GetY() + height;
}

/** Set the size of the rectangle.
 * @param size		New size. */
void Rect::Resize(Size size) {
	Resize(size.GetWidth(), size.GetHeight());
}