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
 * @brief		Surface class.
 */

#include <kernel/area.h>
#include <kernel/object.h>
#include <kernel/status.h>
#include <kernel/vm.h>

#include <kiwi/Support/Utility.h>
#include <kiwi/Error.h>

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "ServerSurface.h"

using namespace kiwi;
using namespace std;

/** Create a surface.
 * @param size		Size of the surface. */
ServerSurface::ServerSurface(Connection *owner, const Size &size) :
	m_owner(owner), m_size(size), m_area(0), m_mapping(0), m_image(0),
	m_cairo(0)
{
	/* Get the size of the area to create. Surfaces are 32-bit ARGB, with
	 * 4 bytes per pixel. */
	size_t data_size = p2align(GetDataSize(), 0x1000);

	/* Create a new area. */
	status_t ret = area_create(data_size, -1, 0, NULL, AREA_READ | AREA_WRITE, &m_area);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		clog << "Failed to create area for surface: " << e.GetDescription() << endl;
		throw e;
	}
}

/** Destroy the surface. */
ServerSurface::~ServerSurface() {
	Unmap();
	handle_close(m_area);
}

/** Get the ID of the surface (the same as its area ID).
 * @return		ID of the surface. */
area_id_t ServerSurface::GetID() const {
	return area_id(m_area);
}

/** Get a pointer to the surface's data.
 * @return		Pointer to surface data, or NULL if unable to map. */
void *ServerSurface::GetData() {
	if(!m_mapping) {
		status_t ret = vm_map(NULL, area_size(m_area), VM_MAP_READ | VM_MAP_WRITE,
		                      m_area, 0, &m_mapping);
		if(ret != STATUS_SUCCESS) {
			return NULL;
		}
	}

	return m_mapping;
}

/** Get the size of the surface's data.
 * @return		Size of the surface's data. */
size_t ServerSurface::GetDataSize() const {
	return (m_size.GetWidth() * m_size.GetHeight() * 4);
}

/** Get a pixman image for the surface.
 * @return		Pointer to pixman image, or NULL on failure. */
pixman_image_t *ServerSurface::GetPixmanImage() {
	if(!m_image) {
		if(!GetData()) {
			return 0;
		}

		m_image = pixman_image_create_bits(PIXMAN_a8r8g8b8, m_size.GetWidth(),
		                                   m_size.GetHeight(),
		                                   reinterpret_cast<uint32_t *>(m_mapping),
		                                   m_size.GetWidth() * 4);
		if(!m_image) {
			return 0;
		}
	}

	return m_image;
}

/** Get a Cairo surface for the surface.
 * @return		Pointer to Cairo surface, or NULL on failure. */
cairo_surface_t *ServerSurface::GetCairoSurface() {
	if(!m_cairo) {
		if(!GetData()) {
			return 0;
		}

		m_cairo = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char *>(m_mapping),
		                                              CAIRO_FORMAT_ARGB32, m_size.GetWidth(),
		                                              m_size.GetHeight(), m_size.GetWidth() * 4);
		if(cairo_surface_status(m_cairo) != CAIRO_STATUS_SUCCESS) {
			return 0;
		}
	}

	return m_cairo;
}

/** Change the size of the surface.
 * @param size		New size.
 * @return		Status code describing result of the operation. */
status_t ServerSurface::Resize(const Size &size) {
	Unmap();

	/* Get the new size for the area. */
	size_t data_size = p2align(size.GetWidth() * size.GetWidth() * 4, 0x1000);

	/* Resize the area. */
	status_t ret = area_resize(m_area, data_size);
	if(ret != STATUS_SUCCESS) {
		// TODO: Workaround for kernel not supporting shrinking.
		if(ret != STATUS_NOT_IMPLEMENTED) {
			return ret;
		}
	}

	m_size = size;
	return STATUS_SUCCESS;
}

/** Unmap the surface. */
void ServerSurface::Unmap() {
	if(m_cairo) {
		cairo_surface_destroy(m_cairo);
		m_cairo = 0;
	}
	if(m_image) {
		pixman_image_unref(m_image);
		m_image = 0;
	}
	if(m_mapping) {
		vm_unmap(m_mapping, area_size(m_area));
		m_mapping = 0;
	}
}