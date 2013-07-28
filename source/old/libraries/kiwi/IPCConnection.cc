/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		IPC connection class.
 */

#include <kernel/object.h>
#include <kernel/ipc.h>

#include <kiwi/IPCConnection.h>

#include <cstdlib>

#include "org.kiwi.ServiceManager.h"
#include "Internal.h"

using namespace kiwi;
using namespace org::kiwi::ServiceManager;

/** Constructor for IPCConnection.
 * @param handle	If not negative, an existing connection handle to make
 *			the object use. Must refer to a connection object. */
IPCConnection::IPCConnection(handle_t handle) {
	if(handle >= 0) {
		if(unlikely(kern_object_type(handle) != OBJECT_TYPE_CONNECTION)) {
			libkiwi_fatal("IPCConnection::IPCConnection: Handle must refer to a connection object.");
		}

		SetHandle(handle);
	}
}

/** Connect to a port.
 *
 * Connects to an IPC port. If the object currently refers to a connection, the
 * old connection will be closed upon success, and the object will refer to the
 * new connection. Upon failure, the old connection will remain open.
 *
 * @param id		Port ID to connect to.

 * @return		True if succeeded in connecting, false if not.
 */
bool IPCConnection::Connect(port_id_t id) {
	handle_t handle;
	status_t ret;

	ret = kern_connection_open(id, &handle);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	SetHandle(handle);
	return true;
}

/** Connect to a port.
 *
 * Connects to an IPC port. If the object currently refers to a connection, the
 * old connection will be closed upon success, and the object will refer to the
 * new connection. Upon failure, the old connection will remain open.
 *
 * @param name		Port name to connect to.
 *
 * @return		True if succeeded in connecting, false if not.
 */
bool IPCConnection::Connect(const char *name) {
	const char *pstr;
	port_id_t id;

	/* Work out the service manager port ID. The ID of the session's
	 * service manager (if any) is set in the environment. */
	pstr = getenv("SVCMGR_PORT");
	if(pstr) {
		id = strtol(pstr, NULL, 10);
	} else {
		id = 0;
	}

	/* Look up the port ID. */
	try {
		status_t ret;

		ServerConnection svcmgr;
		svcmgr.Connect(id);
		ret = svcmgr.LookupPort(name, id);
		if(unlikely(ret != STATUS_SUCCESS)) {
			SetError(ret);
			return false;
		}
	} catch(Error &e) {
		SetError(e);
		return false;
	} catch(RPCError &e) {
		SetError(STATUS_DEST_UNREACHABLE);
		return false;
	}

	return Connect(id);
}

/** Send a message on a port.
 * @param type		Type ID of message to send.
 * @param buf		Data buffer to send.
 * @param size		Size of data buffer.
 * @return		Whether sending the message succeeded. */
bool IPCConnection::Send(uint32_t type, const void *buf, size_t size) {
	status_t ret;

	ret = kern_connection_send(m_handle, type, buf, size);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	return true;
}

/** Receive a message from a port.
 * @param type		Where to store type of message.
 * @param buf		Where to store pointer to data buffer (should be freed
 *			with delete[] when no longer needed).
 * @param size		Where to store size of buffer.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until a message is received, and a timeout of 0 will
 *			return immediately if no messages are waiting to be
 *			received.
 * @return		True if message received, false if an error occurred. */
bool IPCConnection::Receive(uint32_t &type, char *&data, size_t &size, useconds_t timeout) {
	status_t ret;

	ret = kern_connection_peek(m_handle, timeout, &type, &size);
	if(unlikely(ret != STATUS_SUCCESS)) {
		SetError(ret);
		return false;
	}

	data = new char[size];
	ret = kern_connection_receive(m_handle, 0, 0, data, size);
	if(unlikely(ret != STATUS_SUCCESS)) {
		delete[] data;
		SetError(ret);
		return false;
	}

	return true;
}

/** Wait for the remote end to hang up the connection.
 * @param timeout	Timeout in microseconds. A timeout of -1 will block
 *			until the connection is hung up, and a timeout of 0
 *			will return immediately if the connection is not
 *			already hung up.
 * @return		True if successful, false if the timeout expired. */
bool IPCConnection::WaitForHangup(useconds_t timeout) const {
	return (_Wait(CONNECTION_EVENT_HANGUP, timeout) == STATUS_SUCCESS);
}

/** Register events with the event loop. */
void IPCConnection::RegisterEvents() {
	RegisterEvent(CONNECTION_EVENT_HANGUP);
	RegisterEvent(CONNECTION_EVENT_MESSAGE);
}

/** Handle an event on the connection.
 * @param event		Event ID. */
void IPCConnection::HandleEvent(int event) {
	switch(event) {
	case CONNECTION_EVENT_HANGUP:
		OnHangup();
		break;
	case CONNECTION_EVENT_MESSAGE:
		OnMessage();
		break;
	}
}