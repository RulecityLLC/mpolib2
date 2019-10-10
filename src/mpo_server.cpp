/*
 * mpo_server.cpp
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

// mpo_server.cpp
// by Matt Ownby

#include "mpo_server_internal.h"
#include <string.h>	// for memset

#ifdef WIN32
#else
#include <unistd.h>
//#include <sys/ioctl.h>
#include <fcntl.h>
#endif

IMpoServerSPtr MpoServerFactory::CreateInstance()
{
	return IMpoServerSPtr(new MpoServer(), MpoServer::deleter());
}

void MpoServer::Shutdown()
{
	// if our socket is open
	if (m_listening_socket > 0)
	{
		net_close(m_listening_socket);
		m_listening_socket = 0;
	}
}

// Initiallizes the server to a listen for connections on the given port
void MpoServer::Initialize(unsigned int port, const char *cpszHostIP4)
{
	bool result = false;
	
	m_listening_socket = (MPO_SOCKET) socket(AF_INET, SOCK_STREAM, 0);

	// make sure socket worked
	if (m_listening_socket > 0)
	{
		memset(&m_servaddr, 0, sizeof(m_servaddr));
		m_servaddr.sin_family = AF_INET;

		// NULL means to listen on all interfaces
		if (cpszHostIP4 == NULL)
		{
			m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		}
		// otherwise, we listen on a specific interface, indicated by IP address
		else m_servaddr.sin_addr.s_addr = inet_addr(cpszHostIP4);
		m_servaddr.sin_port = htons(port);

		int nonzero = 1;
		// setsockopt so we can immediately bind to this port again when the program execution stops
		if (setsockopt(m_listening_socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &nonzero, sizeof(nonzero)) == 0)
		{
			// bind to a port and verify
			if (bind(m_listening_socket, (const sockaddr *) &m_servaddr, (int) sizeof(m_servaddr)) == 0)
			{
				// listen on that port and verify
				// (5 is the 'backlog')
				if (listen(m_listening_socket, 5) == 0)
				{
					m_initialized = true;
					result = true;
				}
			}
		}
	}
	// else we got some error, and if this becomes a problem figuring out what the error is, we can
	// add more specific error identification later :)
	
	// if socket was opened but our result eventually failed ...
	if ((m_listening_socket > 0) && (!result))
	{
		Shutdown();
	}

	if (!result)
	{
		throw runtime_error("Initialization failed");
	}
}

bool MpoServer::accept_connection(int &socket, struct sockaddr *socket_info, int *length,
				   unsigned int timeout_ms)
{
	
	bool result = false;
	int rv;

	if (m_initialized)
	{
		rv = better_select(m_listening_socket, SELECT_READ, timeout_ms);
		
		// if someone is connecting
		if (rv > 0)
		{
			socket = (int) accept(m_listening_socket, socket_info, (socklen_t *) length);
			// TODO : make this totally non-blocking!

			// if connection was accepted
			if (socket > 0)
			{
				// try to set socket to non-blocking mode
				int ioctl_result = 0;
#ifdef WIN32
				u_long nonblocking_i = 1;
				ioctl_result = ioctlsocket(socket, FIONBIO, &nonblocking_i);
#else
				ioctl_result = fcntl(socket, F_SETFL, O_NONBLOCK);	// turn on nonblocking I/O
#endif
				// if we successfully set the port to non-blocking
				if (ioctl_result == 0)
				{
					result = true;
				}
			}
			// else connection wasn't accepted, but we're returning false anyway
		}
	}
	
	return result;
}

mpo_sockpres_autoptr MpoServer::Accept(unsigned int timeout_ms)
{
	mpo_sockpres_autoptr pRes;	// defaults to NULL
	int socket = 0;
	sockaddr_in info;
	int length = sizeof(info);

	if (accept_connection(socket, (sockaddr *) &info, &length, timeout_ms))
	{
		// make sure length is still what we expect it to be
		if (length == sizeof(info))
		{
			pRes = mpo_socket_presenter::GetInstance(socket, info);
		}
	}

	return pRes;
}
