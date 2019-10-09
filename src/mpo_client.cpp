/*
 * mpo_client.cpp
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

// mpo_client.cpp
// by Matt Ownby

#ifndef WIN32
#include <fcntl.h>
#include <errno.h>	// errno
#endif

#include <string.h>
#include "mpo_client_internal.h"
#include <mpolib2/mpo_net_stream.h>

#ifdef DEBUG
#include <assert.h>
#include <iostream>
#endif

using namespace std;

// convenient constant array
const char CRLF[3] = { 13, 10, 0 };

///////////////////////////////////////////////////////////////

mpo_client::mpo_client()
{
	m_sock = -1;
}

mpo_client::~mpo_client()
{
	disconnect();
}

IMpoClientSPtr MpoClientFactory::CreateInstance()
{
	return IMpoClientSPtr(new mpo_client, mpo_client::deleter());
}

net_result mpo_client::connect_to_host(const char *host, int port, unsigned int timeout_ms)
{
	net_result result = NET_ERROR;
	string host_ip4 = "";

	// just in case net_nslookup fails
	m_sock = -1;

	// Make sure that we can do a DNS lookup of 'host'.
	// (if we are already connected, re-assigning m_SockSPtr will automatically disconnect the previous connection)
	if (net_nslookup(host, host_ip4))
	{
		m_sock = socket(AF_INET, SOCK_STREAM, 0);

		// if socket was created successfully
#ifdef WIN32
		if (m_sock != INVALID_SOCKET)
#else
		if (m_sock != -1)
#endif
		{
			in_addr addr;	// has one member, s_addr

			// convert from ASCII to binary
#ifndef WIN32
			if (inet_pton(AF_INET, host_ip4.c_str(), &addr) > 0)
#else
			addr.s_addr = inet_addr(host_ip4.c_str());
			// win32 doesn't support anything decent apparently, it sucks
			if (addr.s_addr != -1)
#endif
			{
				m_server.sin_family = AF_INET;
				m_server.sin_addr = addr;
				m_server.sin_port = htons((short) port);

				// This now holds our socket info and will be responsible for closing the socket when the time comes.
				m_SockSPtr = mpo_socket_presenter::GetInstance(m_sock, m_server);

#ifndef WIN32
				// turn on non-blocking I/O
				if (fcntl(m_sock, F_SETFL, O_NONBLOCK) == 0)
				{
					if (connect(m_sock, (sockaddr *) &m_server, sizeof(m_server)) == 0)
					{
						result = NET_OK;
					}
					// see if we got a real error or if we're just waiting for the connection
					else
					{
						// NOTE : we could check 'errno' here, but that could be unreliable for multi-threaded programming
						result = wait_connect(timeout_ms);
					}
				} // if non-blocking I/O succeeded
#else
				// WIN32 : turn on non-blocking I/O
				int iMode = 1;	// non-zero means non-blocking
				if (ioctlsocket(m_sock, FIONBIO, (u_long FAR*) &iMode) == 0)
				{
					// if connect succeeded ...
					if (connect(m_sock, (sockaddr *) &m_server, sizeof(m_server)) == 0)
					{
						result = NET_OK;
					} // end if connection succeeded

					// if it fails, we have to assume it's because of NON-BLOCKING reasons
					// because the only way to detect the error is to call the non-thread-safe
					// WSAGetLastError which we don't want to do (yet)
					else
					{
						// 10035 is what we expect to get, it just means to wait longer
						result = wait_connect(timeout_ms);
					}
				}	// end if non-blocking I/O was enabled
				// else we got an error which is our default return value
#endif
			} // end if ip was converted from text to encoded
		} // end if socket succeeded

		// if socket wasn't good
		else
		{
#ifdef WIN32
#ifdef DEBUG
			cout << "Socket could not be created for " << host << ", error code " <<
				numstr::ToStr(WSAGetLastError()) << endl;
#endif // DEBUG
#endif // WIN32
		}

	} // end safety check

	// if socket was created, but we ultimately failed, then disconnect
	if ((m_sock != -1) && (result == NET_ERROR))
	{
#ifdef DEBUG
//		cout << "disconnecting due to connect failure" << endl;
#endif
		disconnect();
	}

	return result;
}

net_result mpo_client::wait_connect(unsigned int timeout_ms)
{
#ifndef WIN32
	net_result result = NET_ERROR;

	// SELECT_WRITE because that's what you do when checking for a connection in UNIX
	int wv = better_select(m_sock, SELECT_WRITE, timeout_ms);

	// if we got something ...
	if (wv > 0)
	{
		unsigned char buf[8];	// holds result, must be at least size of an integer so the rest of our code doesn't break!
		socklen_t optlen = sizeof(buf);

		memset(buf, 0, sizeof(buf));	// clear buffer

		// if SO_ERROR is 0, connection succeeded
		if (getsockopt(m_sock, SOL_SOCKET, SO_ERROR, buf, &optlen) == 0)
		{
			int *iptr = (int *) buf;

			// if there were no errors, it means connection succeeded!
			if (*iptr == 0)
			{
				result = NET_OK;
			}
			// else connect failed
		}
		// else getsockopt failed
	}
	// timeout
	else if (wv == 0)
	{
		result = NET_TIMEOUT;
	}
	// else we got an error
	else
	{
		#ifdef DEBUG
		cout << "select returned < 0 " << endl;
		perror("err: ");
		#endif
	}
#else
	// WIN32 stuff ...
	// NOTE : as you can see, the connection can succeed at any time, but
	// can only fail after the timeout period has expired
	// This could be improved, but who cares?... just use small intervals

	net_result result = NET_TIMEOUT;

	int wv = better_select(m_sock, SELECT_WRITE, timeout_ms);

	if (wv > 0)
	{
		result = NET_OK;
	}
	else if (wv < 0)
	{
		result = NET_ERROR;
	}
	// else they haven't necessarily connected successfully,
	// so let's check to see if their connection failed
	else
	{
		// instant check for connection failure
		wv = better_select(m_sock, SELECT_EXCEPT, 0);
		if (wv > 0)
		{
			result = NET_ERROR;
		}
		else if (wv < 0)
		{
			result = NET_ERROR;
		}
	}
	// else the connection hasn't necessarily failed, so just wait ...

#endif // end WIN32

	return result;
}

// WARNING : don't use this function for anything serious, it's only for quick and dirty testing
net_result mpo_client::connect_and_wait(const char *host_ipv4, int port)
{
	net_result res = connect_to_host(host_ipv4, port, 1000);

	// try to connect for a while ...
	while (res == NET_TIMEOUT)
	{
		res = wait_connect(1000);
	}

	return res;
}

void mpo_client::disconnect()
{
	// This allows the socket to stay open if the caller is using it elsewhere.
	m_SockSPtr.reset();
}

mpo_sockpres_autoptr mpo_client::get_socket_safe()
{
	return m_SockSPtr;
}

nonblocking_sharedptr mpo_client::get_stream()
{
	return MpoNetStreamFactory::CreateInstance(get_socket_safe());
}

//////////////////////////////////////////////////////////////////////////////
