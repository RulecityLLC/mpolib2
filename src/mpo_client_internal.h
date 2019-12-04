//
// Created by Matt on 10/4/2019.
//

#ifndef MPO2_MPO_CLIENT_INTERNAL_H
#define MPO2_MPO_CLIENT_INTERNAL_H

#include <mpolib/mpo_client.h>
#include <mpolib/mpo_net.h>
#include <mpolib/mpo_deleter.h>

// Purpose: So that clients consuming shared lib cannot see our internal implementation

class mpo_client : public IMpoClient, public MpoDeleter
{
    friend class MpoClientFactory;

public:

// Attempts to connect to 'host' (does DNS lookup if necessary)
//  on the port indicated by 'port', using non-blocking I/O
// If connection succeeds within the timeout period, returns NET_OK
// If connection fails within the timeout period, returns NET_ERROR
// If connection hasn't succeeded or failed within the timeout period, returns NET_TIMEOUT
//  in which case, the user should continue to wait for the connection to succeed by
//  calling wait_connect().  disconnect() can be called at any time to close connection.
net_result connect_to_host(const char *host, int port, unsigned int timeout_ms);

// if connect_to_host() has timed out, this function can be called to continue waiting
// for connection to succeed.  The results are either NET_OK, NET_ERROR, or NET_TIMEOUT
// WIN32 NOTE : the connection can succeed at any time, but can only fail after the timeout period has expired
net_result wait_connect(unsigned int timeout_ms);

// same as connect_to_host except it won't return until either a connection has been
// accepted or rejected
// (useful for quick and dirty connections for testing purposes)
net_result connect_and_wait(const char *host_ipv4, int port);

// Disconnects from remote host at any time.
// (The caller does NOT need to call this!  It will be called automatically when the class goes out of scope!)
void disconnect();

// This is how the caller will perform future operations on the socket.
mpo_sockpres_autoptr get_socket_safe();

// Convenience function.  Returns a versatile non-blocking stream class instance.
nonblocking_sharedptr get_stream();
private:

	mpo_client();
	~mpo_client();

	void DeleteInstance() { delete this; }

MPO_SOCKET m_sock;	// our outgoing socket
sockaddr_in m_server;
mpo_sockpres_autoptr m_SockSPtr;
};

#endif //MPO2_MPO_CLIENT_INTERNAL_H
