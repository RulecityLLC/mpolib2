#include <mpolib/mpo_net_stream.h>
#include <mpolib/mpo_timer.h>

size_t MpoNetStream::Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs)
{
	int stRes = 0;
	net_result res = net_receive(m_socket, buf, (int) stBytesToRead, &stRes, uTimeoutMs);
	ProcessNetRes(res);
	return stRes;
}

bool MpoNetStream::IsReadByteWaiting()
{
	bool bRes = false;

	bRes = (better_select(m_socket, SELECT_READ, 0) > 0);

	return bRes;
}

size_t MpoNetStream::Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs)
{
	int stRes = 0;
	net_result res = net_send(m_socket, buf, (int) stBytesToWrite, &stRes, uTimeoutMs);
	ProcessNetRes(res);
	return stRes;
}

StreamMsg MpoNetStream::GetLastMsg()
{
	return m_LastMsg;
}

MpoNetStream::MpoNetStream() :
m_LastMsg(MSG_ERROR),
m_socket(0)
{
}

MpoNetStream *MpoNetStream::CreateInstance(mpo_sockpres_autoptr sock)
{
	MpoNetStream *pInstance = new MpoNetStream();
	pInstance->m_sockSPtr = sock;
	pInstance->m_socket = sock->GetSocket();
	return pInstance;
}

MpoNetStream *MpoNetStream::CreateInstance(MPO_SOCKET socket)
{
	MpoNetStream *pInstance = new MpoNetStream();
	pInstance->m_socket = socket;
	return pInstance;
}

void MpoNetStream::ProcessNetRes(net_result res)
{
	switch (res)
	{
	case NET_OK:
		m_LastMsg = MSG_OK;
		break;
	case NET_TIMEOUT:
		m_LastMsg = MSG_TIMEOUT;
		break;
	case NET_EOF:
	case NET_DISCONNECT:
		m_LastMsg = MSG_END;
		break;
	default:
		m_LastMsg = MSG_ERROR;
		break;
	}
}

///////////

nonblocking_sharedptr MpoNetStreamFactory::CreateInstance(mpo_sockpres_autoptr sock)
{
	MpoNetStream *pInstance = NULL;

	pInstance = MpoNetStream::CreateInstance(sock);

	nonblocking_sharedptr sp(pInstance, MpoNetStream::deleter());

	return sp;
}

bool MpoNetStreamFactory::ChangeSocket(nonblocking_sharedptr pStream, MPO_SOCKET new_socket)
{
	bool bRes = false;

	// NOTE : I decided to use static_cast here because the dynamic_cast requires more overhead.

	MpoNetStream *pNetStream = static_cast<MpoNetStream *>(pStream.get());
	pNetStream->m_socket = new_socket;
	bRes = true;

	return bRes;

}
