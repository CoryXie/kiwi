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
 * @brief		IPC server class.
 */

#include <kiwi/IPCServer.h>
#include <stdexcept>

using namespace kiwi;

/** Initialise the server with a new unnamed port. */
IPCServer::IPCServer() {
	if(!m_port.create()) {
		throw std::runtime_error("Failed to create port");
	}
	m_port.onConnection.connect(this, &IPCServer::_handleConnection);
}

/** Initialise the server with a new named port.
 * @param name		Name to register port with. */
IPCServer::IPCServer(const char *name) {
	if(!m_port.create()) {
		throw std::runtime_error("Failed to create port");
	} else if(!m_port.registerName(name)) {
		throw std::runtime_error("Failed to register port");
	}
	m_port.onConnection.connect(this, &IPCServer::_handleConnection);
}

/** Signal handler for a connection.
 * @param port		Port the connection was received on. */
void IPCServer::_handleConnection(IPCPort *port) {
	handle_t handle = m_port.listen();
	if(handle >= 0) {
		handleConnection(handle);
	}
}
