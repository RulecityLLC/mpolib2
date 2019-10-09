#ifndef MPO_NET_STREAM_H
#define MPO_NET_STREAM_H

#include "mpo_dll.h"
#include "mpo_stream.h"
#include "mpo_net.h"
#include "mpo_socket_presenter.h"

class MpoNetStream : public INonblockingStream, public MpoDeleter
{
	friend class MpoNetStreamFactory;
public:
	size_t Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs);

	bool IsReadByteWaiting();

	size_t Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs);

	StreamMsg GetLastMsg();
private:
	MpoNetStream();

	virtual ~MpoNetStream() {}

	void DeleteInstance() { delete this; }

	static MpoNetStream *CreateInstance(mpo_sockpres_autoptr sock);

	static MpoNetStream *CreateInstance(MPO_SOCKET socket);

	void ProcessNetRes(net_result res);

	StreamMsg m_LastMsg;

	MPO_SOCKET m_socket;

	mpo_sockpres_autoptr m_sockSPtr;
};

class EXPORT_ME MpoNetStreamFactory
{
public:
	// Preferred version of this function because the socket will be automatically closed.
	static nonblocking_sharedptr CreateInstance(mpo_sockpres_autoptr sock);

	// Changes the socket on a stream that's already been instantiated.
	// Returns false if the stream's type doesn't match the pointer.
	static bool ChangeSocket(nonblocking_sharedptr pStream, MPO_SOCKET new_socket);
};

#endif // MPO_NET_STREAM_H

