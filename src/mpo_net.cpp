/*
 * mpo_net.cpp
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

#include <mpolib/mpo_net.h>
#include <mpolib/mpo_timer.h>	// to guarantee that we block the proper amount of time
#include <mpolib/mpo_numstr.h>
#include <assert.h>
#include <stdexcept>

#ifdef DEBUG
#include <iostream>
#endif

#include <string>
using namespace std;

#ifdef WIN32
WSADATA g_wsaData;
#else
#include <signal.h>
#endif

// BEGIN INTERNAL-USE ONLY
int g_dwLastError = 0;
void net_SetLastError(int dwVal)
{
	g_dwLastError = dwVal;
}
// END INTERNAL-USE ONLY

EXPORT_ME string net_GetLastErrorStr()
{
	string strRes;
#ifdef WIN32
	strRes = string("Winsock error ") + numstr::ToStr(g_dwLastError);

	char buf[320];
	DWORD dwRes = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, g_dwLastError, 0, buf, sizeof(buf), NULL);
	// if FormatMessage worked (it should)
	if (dwRes > 0)
	{
		strRes += string(": ") + string(buf, dwRes);
	}
	// else just settle for the error number
#else
	// UNIX
	// TODO : get a nice error string for this
	strRes = string("Unix error ") + numstr::ToStr((int) g_dwLastError);
#endif // platform

	return strRes;
}

// NOT THREAD SAFE!  Call at the beginning of the program
EXPORT_ME void net_init()
{
#ifdef WIN32
	WSAStartup(MAKEWORD(1,1), &g_wsaData );
#else
	signal(SIGPIPE, SIG_IGN);	// ignore SIGPIPE for UNIX
#endif
}

// NOT THREAD SAFE!  Call at the end of the program
EXPORT_ME void net_shutdown()
{
#ifdef WIN32
	WSACleanup();
#endif
}

EXPORT_ME net_result net_send(MPO_SOCKET socket, const void *buffer, int bytes_to_send, int *bytes_sent, unsigned int timeout_ms)
{
	net_result result = NET_ERROR;

	// force user to pay attention to 'bytes_sent', this function can't be used properly otherwise
	if (bytes_sent == NULL) return NET_ERROR;

	*bytes_sent = 0;	// initialize

#ifdef DEBUG
	// If we're in debug mode, then decrease the bytes to send by 1 in order to test
	//  out our net_send_and_block function.
	if (bytes_to_send > 1)
	{
		--bytes_to_send;	// testing blocking function ...
	}
#endif // DEBUG

	// safety check
	if (bytes_to_send > 0)
	{
		int wv = better_select(socket, SELECT_WRITE, timeout_ms);
		
		// if we didn't time out, then we know that we won't block when we write
		if (wv > 0)
		{
			*bytes_sent = send(socket, (const char *) buffer, bytes_to_send, 0);
			
			// we should always be able to send at least 1 byte because select returned > 0
			if (*bytes_sent > 0)
			{
				result = NET_OK;
			}
			// if we didn't send anything (this shouldn't happen)
			else if (*bytes_sent == 0)
			{
				assert(false);	// so we can see what's going on
				result = NET_TIMEOUT;
			}
			// there is an error
			else
			{
				result = NET_ERROR;
#ifdef WIN32
				int iLastErr = WSAGetLastError();

				// this isn't supposed to happen after we call better_select,
				//  but it can happen on win98.. it's not an error, so
				//  we return timeout.
				if (iLastErr == WSAEWOULDBLOCK) result = NET_TIMEOUT;

				// else set the error and fail
				else net_SetLastError(iLastErr);
#else
				// unix
				net_SetLastError(*bytes_sent);
#endif
			}
		}	// end if we won't block
		// if we timed out
		else if (wv == 0)
		{
			result = NET_TIMEOUT;
		}
		// else we got some other error.  better_select will already set the error code.

	} // end safety check

	return result;
}


EXPORT_ME net_result net_receive(MPO_SOCKET socket, void *buffer, int bytes_can_read, int *bytes_read, unsigned int timeout_ms)
{
	net_result result = NET_ERROR;
	int rv = 0;
	int bytes_read_replacement = 0;

	// if user passed in NULL for bytes_read, it means they don't care what the result is,
	// so we'll provide a dummy variable to hold it
	if (bytes_read == NULL) bytes_read = &bytes_read_replacement;
	*bytes_read = 0;
	
	// safety check
	if (bytes_can_read > 0)
	{
		rv = better_select(socket, SELECT_READ, timeout_ms);
		
		// if we didn't time out, then we know that our socket triggered the event
		if (rv > 0)
		{
			int new_bytes = 0;	// the # of new bytes we will read
			new_bytes = recv(socket, (char *) buffer, bytes_can_read, 0);

			// if we got some stuff
			if (new_bytes > 0)
			{
				result = NET_OK;
				*bytes_read = new_bytes;
			}

			// if we read '0' bytes, it means there was a disconnect
			else if (new_bytes == 0)
			{
				result = NET_DISCONNECT;
			}
			// else there was an error, but NET_ERROR is the default case anyway.
			// NOTE : we should never get an error due to our socket being in non-blocking mode,
			//		because we called select first!  However, some implementations may be broken in this regard.
			// If we can determine if this happens, we should return NET_TIMEOUT instead of an error.
			else
			{
#ifdef WIN32
				int iLastErr = WSAGetLastError();

				// this should never happen.  If it is happening, I want to know about it. :)
				assert (iLastErr != WSAEWOULDBLOCK);

				net_SetLastError(iLastErr);
#else
				net_SetLastError(result);
#endif
			}
		}
		// else if we timed out ..
		else if (rv == 0)
		{
			result = NET_TIMEOUT;
		}
		// else we got an error, but NET_ERROR is our default case anyway
		// NOTE : better_select will have already set the last error.
	} // end if variables check out

	// else we got some bogus values so we'll return error anyway ...

	return result;
}

EXPORT_ME void net_close(MPO_SOCKET socket)
{
#ifdef WIN32
		int iRes = closesocket(socket);

		// If closesocket got an error, it probably means that it was called more than once on the same socket.
		// This is a code defect, so we want an assert so we can find and fix all outstanding defects.
		if (iRes == SOCKET_ERROR)
		{
			int iErr = WSAGetLastError();
			net_SetLastError(iErr);
			assert(false);
		}

#else
		int iRes = close(socket);
		if (iRes != 0)
		{
			assert(iRes == 0);
		}
#endif
}

EXPORT_ME bool net_nslookup(const char *host, string &ipv4)
{
	bool result = false;
	struct hostent *info = NULL;

	ipv4 = "";

#ifdef WIN32
	// gethostbyname may not be thread-safe!
	// some document on MSDN claims all winsock calls are thread-safe, but... I'm not so sure :)
	info = gethostbyname(host);
#else
	info = gethostbyname(host);	// temporary unsafe!
#endif
	
	// if the DNS resolution worked
	if (info)
	{
		result = true;
		
		// unix has a function to do this automatically, but windows doesn't (or does it?)
		// so we have to do it manually ... (yes, it's retarded, I know)
		ipv4 = numstr::ToStr((unsigned char) info->h_addr_list[0][0]) + ".";
		ipv4 += numstr::ToStr((unsigned char) info->h_addr_list[0][1]) + ".";
		ipv4 += numstr::ToStr((unsigned char) info->h_addr_list[0][2]) + ".";
		ipv4 += numstr::ToStr((unsigned char) info->h_addr_list[0][3]);
	}

	return result;
}

EXPORT_ME bool net_parse_url(const string &url, string &host, unsigned int &port, string &uri)
{
	bool result = false;

	port = 80;	// default
	uri = "/";	// default

	// make sure the header is present (make case-insensitive later)
	if (url.substr(0, 7) == "http://")
	{
		int port_start = (int) url.find(':', 7);	// check to see if a port may be specified
		int uri_start = (int) url.find('/', 7);	// find where the uri starts

		// check for possible port entry
		if (port_start > 7)
		{
			host = url.substr(7, port_start - 7);	// get host while we're at it

			if (uri_start > 7)
			{
				// if a URI is specified, and at least 1 character is between it and the colon
				if (uri_start > (port_start + 1))
				{
					port = numstr::ToUint32(url.substr(port_start, (uri_start - port_start)).c_str());
				}
				uri = url.substr(uri_start);
			}
			// if there is at least 1 character after the colon
			else if (url.size() > ((unsigned int) port_start + 1))
			{
				port = numstr::ToUint32(url.substr(port_start, (url.size() - port_start)).c_str());
			}
		}
		// else if no port, just host and URI
		else if (uri_start > 7)
		{
			host = url.substr(7, uri_start - 7);
			uri = url.substr(uri_start);
		}
		// else no URI, no port, just the host
		else
		{
			host = url.substr(7);
		}

		result = true;
	}

	return result;
}

EXPORT_ME string GetIpFromSocket(const sockaddr *pSocket)
{
#ifdef WIN32
	// ipv4-only support for now
	return inet_ntoa(((sockaddr_in *) pSocket)->sin_addr);
#else
	char s[80];

	switch(pSocket->sa_family)
	{
	case AF_INET:
		inet_ntop(AF_INET, &(((struct sockaddr_in *)pSocket)->sin_addr),
			s, sizeof(s));
		break;

	case AF_INET6:
		inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)pSocket)->sin6_addr),
			s, sizeof(s));
		break;

	default:
		throw runtime_error("Unknown AF type");
	}
	return s;
#endif
}

EXPORT_ME int better_select(MPO_SOCKET socket, select_type which_set, unsigned int timeout_ms)
{
	int result = -1;
	fd_set *rset = NULL, *wset = NULL, *eset = NULL;
	fd_set the_set;
	timeval timeout;

	switch (which_set)
	{
	case SELECT_READ:
		rset = &the_set;
		break;
	case SELECT_WRITE:
		wset = &the_set;
		break;
	case SELECT_EXCEPT:
		eset = &the_set;
		break;
	default:
		// error!
		break;
	}

	// safety check
	if (rset || wset || eset)
	{
		unsigned int timer = MpoTimerUtil::RefreshTimer();
		timeout.tv_sec = timeout_ms / 1000;
		timeout.tv_usec = (timeout_ms % 1000) * 1000;

		// we don't ever expect to loop here, but another socket can trigger
		// select to return (even if the socket assigned to select hasn't returned)
		for (;;)
		{
			FD_ZERO(&the_set);
			FD_SET(socket, &the_set);
			result = select((int) socket + 1, rset, wset, eset, &timeout);

			// if another socket triggered the 'event' ...
			if ((result > 0) && (!FD_ISSET(socket, &the_set)))
			{
				// and if we've timed out, then set 'result' to timeout status and exit the loop
				if (MpoTimerUtil::GetElapsedMs(timer) > timeout_ms)
				{
					result = 0;
					break;
				}
				else
				{
					MpoTimerUtil::MakeDelay(1);	// else sleep in case select is returning instantly
				}
			}
			// else if this socket triggered the event or if we timed out or got an error,
			// then exit the loop
			else
			{
				break;
			}
		} // end loop
	} // end safety check

	// if we got an error, set the error code
	if (result < 0)
	{
#ifdef WIN32
		net_SetLastError(WSAGetLastError());
#else
		// in unix, the error _is_ the result
		net_SetLastError(result);
#endif // WIN32
	}

	return result;
}
