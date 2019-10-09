/*
 * mpo_server.h
 *
 * Copyright (C) 2005 Matthew P. Ownby
 *
 * This file is part of MPOLIB, a multi-purpose library
 *
 * MPOLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPOLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MPO's NOTE:
//  I may wish to use MPOLIB in a proprietary product some day.  Therefore,
//   the only way I can accept other people's changes to my code is if they
//   give me full ownership of those changes.

// mpo_server.h
// by Matt Ownby

#ifndef MPO_SERVER_H
#define MPO_SERVER_H

#include "mpo_dll.h"
#include "mpo_net.h"
#include "mpo_socket_presenter.h"

using namespace std;

class EXPORT_ME mpo_server
{
public:
	mpo_server();
	~mpo_server();

	void shutdown();
	
	// initializes a socket for incoming TCP connections by binding and listening on the port specified.
	// 'cpszHostIP4' should be the IP address of the interface to listen on.. NULL means to listen on all interfaces
	// Returns true on success or false on error.
	bool initialize(unsigned int port, const char *cpszHostIP4 = NULL);

private:
	// Polls for a new incoming connection and if it finds one, it populates socket, and socket_info
	// with relevant data and returns true.  Socket will contain the new socket of the accepted connection.
	// 'length' must be passed in as the sizeof the sockaddr_in struct (not sockaddr),
	// and it will be returned as the final size of the structure.
	// There really is a reason for this, so be careful about changing it.
	// Returns false if there is no new connection waiting to be accepted.
	bool accept_connection(int &socket, struct sockaddr *socket_info,
		int *length, unsigned int timeout_ms);

public:
	// A less confusing way to accept connections.
	// Returns an auto-pointer to an instance of mpo_socket_presenter (makes getting info easier) on success,
	//  or NULL if no connection is present.
	mpo_sockpres_autoptr accept_connection(unsigned int timeout_ms);

private:
	bool m_initialized;	// keeps track of whether we are initialized or not
	MPO_SOCKET m_listening_socket;	// the parent listening socket
	struct sockaddr_in m_servaddr;	// listening socket info
};

#endif // MPO_SERVER_H
