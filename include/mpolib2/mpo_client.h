/*
 * mpo_client.h
 *
 * Copyright (C) 2019 Matthew P. Ownby
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

#ifndef MPO_CLIENT_H
#define MPO_CLIENT_H

#include "mpo_dll.h"
#include "mpo_net.h"	// for #includes
#include "mpo_stream.h"
#include "mpo_socket_presenter.h"
#include "mpo_deleter.h"

class IMpoClient
{
public:
    // Attempts to connect to 'host' (does DNS lookup if necessary)
    //  on the port indicated by 'port', using non-blocking I/O
    // If connection succeeds within the timeout period, returns NET_OK
    // If connection fails within the timeout period, returns NET_ERROR
    // If connection hasn't succeeded or failed within the timeout period, returns NET_TIMEOUT
    //  in which case, the user should continue to wait for the connection to succeed by
    //  calling wait_connect().  disconnect() can be called at any time to close connection.
    virtual net_result connect_to_host(const char *host, int port, unsigned int timeout_ms) = 0;

    // if connect_to_host() has timed out, this function can be called to continue waiting
    // for connection to succeed.  The results are either NET_OK, NET_ERROR, or NET_TIMEOUT
    // WIN32 NOTE : the connection can succeed at any time, but can only fail after the timeout period has expired
    virtual net_result wait_connect(unsigned int timeout_ms) = 0;

    // same as connect_to_host except it won't return until either a connection has been
    // accepted or rejected
    // (useful for quick and dirty connections for testing purposes)
    virtual net_result connect_and_wait(const char *host_ipv4, int port) = 0;

    // Disconnects from remote host at any time.
    // (The caller does NOT need to call this!  It will be called automatically when the class goes out of scope!)
//    virtual void disconnect() = 0;

    // This is how the caller will perform future operations on the socket.
    virtual mpo_sockpres_autoptr get_socket_safe() = 0;

    // Convenience function.  Returns a versatile non-blocking stream class instance.
    virtual nonblocking_sharedptr get_stream() = 0;
};

typedef shared_ptr<IMpoClient> IMpoClientSPtr;

class EXPORT_ME MpoClientFactory
{
public:
    static IMpoClientSPtr CreateInstance();
};

#endif
