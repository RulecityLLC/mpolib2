#include <mpolib2/mpo_socket_presenter.h>

mpo_sockpres_autoptr mpo_socket_presenter::GetInstance(MPO_SOCKET socket, const sockaddr_in &socket_info)
{
	mpo_sockpres_autoptr instance(new mpo_socket_presenter(socket, socket_info), mpo_socket_presenter::deleter());
	return instance;
}

MPO_SOCKET mpo_socket_presenter::GetSocket()
{
	return m_socket;
}

unsigned int mpo_socket_presenter::GetPort()
{
	return htons(m_socket_info.sin_port);
}

string mpo_socket_presenter::GetIPString()
{
	return inet_ntoa(m_socket_info.sin_addr);
}

mpo_socket_presenter::mpo_socket_presenter(MPO_SOCKET socket, const sockaddr_in &socket_info) :
m_socket(socket),
m_socket_info(socket_info)
{
}

mpo_socket_presenter::~mpo_socket_presenter()
{
	// I've decided that I don't want to have to manually call net_close on every socket because this feels similar to have to call delete on every memory allocation.
	// Therefore, I want this class to be the vehicle by which all sockets are automatically closed.
	net_close(m_socket);
}

void mpo_socket_presenter::DeleteInstance()
{
	delete this;
}
