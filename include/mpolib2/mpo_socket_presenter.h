#ifndef MPO_SOCKET_PRESENTER_H
#define MPO_SOCKET_PRESENTER_H

#include "mpo_net.h"
#include "mpo_dll.h"
#include <string>
#include "mpo_deleter.h"

using namespace std;

class mpo_socket_presenter;

typedef shared_ptr<mpo_socket_presenter> mpo_sockpres_autoptr;

class EXPORT_ME mpo_socket_presenter : public MpoDeleter
{
	friend class MpoNetStream;
	friend class MpoSSLStream;
public:
	// This is the only way to get an instance of this class
	static mpo_sockpres_autoptr GetInstance(MPO_SOCKET socket, const sockaddr_in &socket_info);

	unsigned int GetPort();
	string GetIPString();

private:

	// I don't want general programs to be able to get the socket itself because then they can do nasty things like close it.
	// Classes that need the socket (such as mpo_net_stream) should be marked as a friend of this class.
	MPO_SOCKET GetSocket();

	// constructor is private to force people to use GetInstance
	mpo_socket_presenter(MPO_SOCKET socket, const sockaddr_in &socket_info);
	~mpo_socket_presenter();

	void DeleteInstance();

	MPO_SOCKET m_socket;
	sockaddr_in m_socket_info;
};

#endif // MPO_SOCKET_PRESENTER_H
