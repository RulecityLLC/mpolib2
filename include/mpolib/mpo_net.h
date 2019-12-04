/*
 * mpo_net.h
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

#ifndef NETWORK_H
#define NETWORK_H

#include "mpo_dll.h"
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef int socklen_t; // winsock has no socklen_t
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#include <string>
using namespace std;

enum net_result
{
	NET_OK,	// success (data was sent/received)
	NET_ERROR,	// error
	NET_TIMEOUT,	// we timed out before accomplishing our goal
	NET_DISCONNECT,	// remote host disconnected
	NET_OVERFLOW,	// if we overflowed our buffer involuntarily
	NET_EOF	// convenience for higher level protocols (HTTP)
};

#ifdef WIN32
#define MPO_SOCKET SOCKET
#else
#define MPO_SOCKET int
#endif

// returns a string describing the last error received
EXPORT_ME string net_GetLastErrorStr();

// initializes networking (only really needed for WinSock)
// NOT THREAD SAFE!
EXPORT_ME void net_init();

// shutdowns down networking (only needed for WinSock)
// NOT THREAD SAFE!
EXPORT_ME void net_shutdown();

// attempts bytes to the specified socket
// 'bytes_sent' indicates how many bytes were actually sent (0 if TCP buffer is full)
// NET_OK means at least 1 byte was sent, NET_TIMEOUT means we timed out before any bytes were sent
EXPORT_ME net_result net_send(MPO_SOCKET socket, const void *buffer, int bytes_to_send, int *bytes_sent, unsigned int timeout_ms);

// receives bytes from the TCP port, but the buffer isn't necessarily filled!
// NET_OK means we got at least 1 byte
EXPORT_ME net_result net_receive(MPO_SOCKET socket, void *buffer, int bytes_can_read, int *bytes_read, unsigned int timeout_ms);

// closes socket
EXPORT_ME void net_close(MPO_SOCKET socket);

// returns IP address as a dotted quad string ("127.0.0.1") using 'host' as input
// WARNING : NOT THREAD SAFE (yet)
EXPORT_ME bool net_nslookup(const char *host, string &ipv4);


// takes 'url' and splits it into 'host' and 'uri'
// example:
// url = "http://www.cnn.com:1025/index.html"
// host = "www.cnn.com"
// port = 1025
// uri = "/index.html"
////////////////////////////
// other valid URL's:
// "http://www.cnn.com:/"
// "http://www.cnn.com/"
// "http://www.cnn.com"
//////////////////////////////
// invalid URL
// "www.cnn.com"
//////////////////////////////
// returns 'true' on success
EXPORT_ME bool net_parse_url(const string &url, string &host, unsigned int &port, string &uri);

// Returns an IP address as a string (supports either IPv4 or IPv6!)
// NOTE: throws exception on failure
EXPORT_ME string GetIpFromSocket(const sockaddr *pSocket);

// constants so we don't have to pass in integers to 'better_select'
enum select_type
{
	SELECT_READ = 0, SELECT_WRITE, SELECT_EXCEPT
};

// a better version of 'select' because it doesn't return if another non-related socket triggered event!
// which_set: 0-read, 1-write, 2-exception
// returns > 0 if there is data waiting to be acted upon
// 0 if timeout has occurred
// < 0 if there is an error
EXPORT_ME int better_select(MPO_SOCKET socket, select_type which_set, unsigned int timeout_ms);

#endif
