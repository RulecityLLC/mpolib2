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

class IMpoServer
{
public:
	// initializes a socket for incoming TCP connections by binding and listening on the port specified.
	// 'cpszHostIP4' should be the IP address of the interface to listen on.. NULL means to listen on all interfaces
	// Throws exception on error.
	virtual void Initialize(unsigned int port, const char *cpszHostIP4 = NULL) = 0;

	// How to accept incoming connections.
	// Returns an auto-pointer to an instance of mpo_socket_presenter (makes getting info easier) on success,
	//  or an NULL'd pointer if no connection is present.
	virtual mpo_sockpres_autoptr Accept(unsigned int timeout_ms) = 0;
};

typedef shared_ptr<IMpoServer> IMpoServerSPtr;

class EXPORT_ME MpoServerFactory
{
public:
	static IMpoServerSPtr CreateInstance();
};

#endif // MPO_SERVER_H
