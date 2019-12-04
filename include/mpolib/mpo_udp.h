#ifndef MPO_UDP_H
#define MPO_UDP_H

#include "mpo_deleter.h"
#include "mpo_dll.h"
#include "mpo_net.h"

#ifdef WIN32
#include <winsock2.h>
#endif // windows

class MpoUdpSender;

typedef shared_ptr<MpoUdpSender> MpoUdpSenderSPtr;

class EXPORT_ME MpoUdpSender : public MpoDeleter
{
public:
	static MpoUdpSenderSPtr GetInstance(unsigned int uPort, const string &strPeerIP, bool bBroadcast);

	bool SendPacket(const string &strPacket);
private:
	MpoUdpSender(unsigned int uPort, const string &strPeerIP, bool bBroadcast);
	~MpoUdpSender();

	void DeleteInstance();

	bool Init();

	void Shutdown();

	unsigned int m_uPort;
	string m_strPeerIP;
	bool m_bBroadcast;
	MPO_SOCKET m_iSocket;

	sockaddr_in m_recvaddr;
};

class MpoUdpReceiver;

typedef shared_ptr<MpoUdpReceiver> MpoUdpReceiverSPtr;

class EXPORT_ME MpoUdpReceiver : public MpoDeleter
{
public:
	static MpoUdpReceiverSPtr GetInstance(unsigned int uPort);

	net_result RecvPacket(string &strPacket, string &strSrcIP, unsigned int uTimeoutMs);

private:
	MpoUdpReceiver(unsigned int uPort);
	~MpoUdpReceiver();

	void DeleteInstance();

	bool Init();

	void Shutdown();

	unsigned int m_uPort;
	MPO_SOCKET m_iSocket;
	sockaddr_in m_addr;
};

////////////////////////////

class IMpoUdp
{
public:

	// if we want to send a packet to a different host than our default
	virtual void SendPacketToEx(const string &strPacket, const string &strDstIP, unsigned int uDstPort) = 0;

	virtual net_result RecvPacket(string *pstrPacket) = 0;
};

typedef shared_ptr<IMpoUdp> IMpoUdpSPtr;

class EXPORT_ME MpoUdp : public IMpoUdp, public MpoDeleter
{
public:
	static IMpoUdpSPtr GetInstance(unsigned int uSrcPort, bool bBroadcast);

	// throws runtime_error exception on failure
	void SendPacketToEx(const string &strPacket, const string &strDstIP, unsigned int uDstPort);

	net_result RecvPacket(string *pstrPacket);
private:
	MpoUdp(unsigned int uSrcPort, bool bBroadcast);
	~MpoUdp();

	void DeleteInstance();

	// throws runtime_error exception on failure
	void Init();

	void Shutdown();

	///////////////////////////////////

	unsigned int m_uSrcPort;
	bool m_bBroadcast;
	MPO_SOCKET m_iSocket;

	sockaddr_in m_sendaddr;
};

/////////

// The purpose of this factory is so we can mock out MpoUdp for unit testing

class IMpoUdpFactory
{
public:
	virtual IMpoUdpSPtr GetInstance(unsigned int uSrcPort, bool bBroadcast) = 0;
};

class MpoUdpFactory : public IMpoUdpFactory
{
public:
	IMpoUdpSPtr GetInstance(unsigned int uSrcPort, bool bBroadcast);
};

#endif // MPO_UDP_H
