//
// Created by Matt on 10/9/2019.
//

#ifndef MPO2_MPO_SERVER_INTERNAL_H
#define MPO2_MPO_SERVER_INTERNAL_H

#include <mpolib/mpo_deleter.h>
#include <mpolib/mpo_server.h>

class MpoServer : public IMpoServer, public MpoDeleter
{
	friend class MpoServerFactory;
public:

	// see interface for usage
	void Initialize(unsigned int port, const char *cpszHostIP4 = NULL);

	// see interface for usage
	mpo_sockpres_autoptr Accept(unsigned int timeout_ms);

private:

	MpoServer() :
	m_initialized(false),
	m_listening_socket(0)
	{}

	virtual ~MpoServer() { Shutdown(); }

	void DeleteInstance() { delete this; }

	void Shutdown();

	// Polls for a new incoming connection and if it finds one, it populates socket, and socket_info
	// with relevant data and returns true.  Socket will contain the new socket of the accepted connection.
	// 'length' must be passed in as the sizeof the sockaddr_in struct (not sockaddr),
	// and it will be returned as the final size of the structure.
	// There really is a reason for this, so be careful about changing it.
	// Returns false if there is no new connection waiting to be accepted.
	bool accept_connection(int &socket, struct sockaddr *socket_info,
						   int *length, unsigned int timeout_ms);

	bool m_initialized;    // keeps track of whether we are initialized or not
	MPO_SOCKET m_listening_socket;    // the parent listening socket
	struct sockaddr_in m_servaddr;    // listening socket info
};

#endif //MPO2_MPO_SERVER_INTERNAL_H
