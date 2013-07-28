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
 * @brief		Graphics surface class.
 */

#include <kernel/area.h>
#include <kernel/object.h>
#include <kernel/vm.h>

#include <kiwi/Graphics/Surface.h>
#include <kiwi/Support/Utility.h>
#include <kiwi/Error.h>

#include <algorithm>
#include <memory>

#include "Internal.h"

using namespace kiwi;
using namespace std;

/** Internal data for Surface. */
struct kiwi::SurfacePrivate {
	SurfacePrivate() : area(-1), handle(-1), mapping(0), cairo(0) {}

	~SurfacePrivate() {
		if(cairo) {
			cairo_surface_destroy(cairo);
		}
		if(mapping) {
			kern_vm_unmap(mapping, kern_area_size(area));
		}
		if(handle >= 0) {
			kern_handle_close(handle);
		}
	}

	area_id_t area;			/**< Area backing the surface (-1 for local surface). */
	handle_t handle;		/**< Handle to area. */
	unsigned char *mapping;		/**< Mapping of the area data. */
	cairo_surface_t *cairo;		/**< Cairo surface. */
};

/** Create a new local surface.
 * @param size		Size of the surface. */
Surface::Surface(Size size) : m_priv(0) {
	std::unique_ptr<SurfacePrivate> priv(new SurfacePrivate());

	priv->cairo = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                         size.GetWidth(),
	                                         size.GetHeight());
	if(cairo_surface_status(priv->cairo) != CAIRO_STATUS_SUCCESS) {
		libkiwi_warn("Surface::Surface: Failed to create Cairo surface: %s.",
		             cairo_status_to_string(cairo_surface_status(priv->cairo)));
		throw Error(STATUS_NO_MEMORY);
	}

	m_priv = priv.release();
}

/** Create a surface referring to a window server surface.
 * @param area		Area ID of the surface. */
Surface::Surface(area_id_t area) : m_priv(0) {
	status_t ret;

	/* Get the surface size. */
	WindowServer::Size size;
	ret = WSConnection::Instance()->GetSurfaceSize(area, size);
	if(ret != STATUS_SUCCESS) {
		throw Error(ret);
	}

	std::unique_ptr<SurfacePrivate> priv(new SurfacePrivate());
	priv->area = area;

	/* Open a handle to the area. */
	ret = kern_area_open(area, AREA_RIGHT_READ | AREA_RIGHT_WRITE, &priv->handle);
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		libkiwi_warn("Surface::Surface: Failed to open surface area: %s.",
		             e.GetDescription());
		throw e;
	}

	/* Map it in. */
	ret = kern_vm_map(NULL, kern_area_size(priv->handle), VM_MAP_READ | VM_MAP_WRITE, priv->handle,
	                  0, reinterpret_cast<void **>(&priv->mapping));
	if(ret != STATUS_SUCCESS) {
		Error e(ret);
		libkiwi_warn("Surface::Surface: Failed to map surface area: %s.",
		             e.GetDescription());
		throw e;
	}

	/* Create the Cairo surface. */
	priv->cairo = cairo_image_surface_create_for_data(priv->mapping, CAIRO_FORMAT_ARGB32,
	                                                  size.width, size.height,
	                                                  size.width * 4);
	if(cairo_surface_status(priv->cairo) != CAIRO_STATUS_SUCCESS) {
		libkiwi_warn("Surface::Surface: Failed to create Cairo surface: %s.",
		             cairo_status_to_string(cairo_surface_status(priv->cairo)));
		throw Error(STATUS_NO_MEMORY);
	}

	m_priv = priv.release();
}

/** Destroy the surface. */
Surface::~Surface() {
	if(m_priv) {
		delete m_priv;
	}
}

/** Get the size of the surface.
 * @return		Size of the surface. */
Size Surface::GetSize() const {
	int width = cairo_image_surface_get_width(m_priv->cairo);
	int height = cairo_image_surface_get_height(m_priv->cairo);
	return Size(width, height);
}

/** Resize the surface.
 * @note		This may destroy the surface content.
 * @fixme		Ideally a resizing failure should leave the surface in
 *			a usable state.
 * @param size		New size for the surface.
 * @return		Whether succeeded in resizing. */
bool Surface::Resize(Size size) {
	cairo_surface_t *cairo;

	if(m_priv->area >= 0) {
		size_t prev = kern_area_size(m_priv->handle);
		unsigned char *mapping;
		status_t ret;

		/* Get the window server to resize the surface. */
		WindowServer::Size _size = { size.GetWidth(), size.GetHeight() };
		ret = WSConnection::Instance()->ResizeSurface(m_priv->area, _size);
		if(ret != STATUS_SUCCESS) {
			return false;
		}

		/* Remap the surface. */
		ret = kern_vm_map(NULL, kern_area_size(m_priv->handle), VM_MAP_READ | VM_MAP_WRITE,
		                  m_priv->handle, 0, reinterpret_cast<void **>(&mapping));
		if(ret != STATUS_SUCCESS) {
			return false;
		}

		swap(m_priv->mapping, mapping);
		kern_vm_unmap(mapping, prev);

		/* Create a new Cairo surface. */
		cairo = cairo_image_surface_create_for_data(m_priv->mapping,
		                                            CAIRO_FORMAT_ARGB32,
	                                                    size.GetWidth(),
	                                                    size.GetHeight(),
		                                            size.GetWidth() * 4);
		if(cairo_surface_status(m_priv->cairo) != CAIRO_STATUS_SUCCESS) {
			return false;
		}

		swap(m_priv->cairo, cairo);
		cairo_surface_destroy(cairo);
	} else {
		/* Just create a new Cairo surface. */
		cairo = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
	                                           size.GetWidth(),
	                                           size.GetHeight());
		if(cairo_surface_status(m_priv->cairo) != CAIRO_STATUS_SUCCESS) {
			return false;
		}

		swap(m_priv->cairo, cairo);
		cairo_surface_destroy(cairo);
	}

	return true;
}

/** Get the surface's raw data.
 * @return		Pointer to raw surface data. This must not be freed or
 *			deleted. */
uint32_t *Surface::GetData() {
	return reinterpret_cast<uint32_t *>(cairo_image_surface_get_data(m_priv->cairo));
}

/** Get the size of the surface's raw data.
 * @return		Size of the raw surface data. */
size_t Surface::GetDataSize() const {
	return GetSize().GetWidth() * GetSize().GetHeight() * 4;
}

/** Get a Cairo surface referring to the surface.
 * @return		Cairo surface for surface. This surface is owned by the
 *			object, and should not be destroyed. */
cairo_surface_t *Surface::GetCairoSurface() {
	return m_priv->cairo;
}