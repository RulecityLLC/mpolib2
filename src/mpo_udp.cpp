#include <mpolib2/mpo_udp.h>
#include <mpolib2/mpo_numstr.h>
#include <string.h>	// for memset
#include <stdexcept>

MpoUdpSenderSPtr MpoUdpSender::GetInstance(unsigned int uPort, const string &strPeerIP, bool bBroadcast)
{
	MpoUdpSenderSPtr pRes;
	MpoUdpSender *pInstance = new MpoUdpSender(uPort, strPeerIP, bBroadcast);
	if (pInstance)
	{
		if (pInstance->Init())
		{
			pRes = MpoUdpSenderSPtr(pInstance, MpoUdpSender::deleter());
		}
		else
		{
			delete pInstance;
		}
	}

	return pRes;
}

bool MpoUdpSender::SendPacket(const string &strPacket)
{
	bool bRes = false;
	int iNumBytes = sendto(m_iSocket, strPacket.data(), strPacket.size(), 0, (struct sockaddr *) &m_recvaddr, sizeof(m_recvaddr));
	if (iNumBytes >= 0)
	{
		bRes = true;
	}

	return bRes;

}

////////////////

MpoUdpSender::MpoUdpSender(unsigned int uPort, const string &strPeerIP, bool bBroadcast) :
m_uPort(uPort),
m_strPeerIP(strPeerIP),
m_bBroadcast(bBroadcast)
{
}

MpoUdpSender::~MpoUdpSender()
{
	Shutdown();
}

void MpoUdpSender::DeleteInstance()
{
	delete this;
}

bool MpoUdpSender::Init()
{
	bool bRes = false;

	try
	{
		if((m_iSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1)
		{
			throw false;
		}

		if (m_bBroadcast)
		{
#ifdef WIN32
			char broadcast = 1;	// char for winsock compatibility
#else
			int broadcast = 1;
#endif
			if((setsockopt(m_iSocket,SOL_SOCKET,SO_BROADCAST,
				&broadcast,sizeof (broadcast))) == -1)
			{
				throw false;
			}
		}

		m_recvaddr.sin_family = AF_INET;
		m_recvaddr.sin_port = htons(m_uPort);
		m_recvaddr.sin_addr.s_addr = inet_addr(m_strPeerIP.c_str());
		memset(m_recvaddr.sin_zero,'\0',sizeof(m_recvaddr.sin_zero));

		bRes = true;
	}
	catch (...)
	{
	}

	return bRes;
}

void MpoUdpSender::Shutdown()
{
	net_close(m_iSocket);
}

///////////////////////////////////////////////

MpoUdpReceiverSPtr MpoUdpReceiver::GetInstance(unsigned int uPort)
{
	MpoUdpReceiverSPtr pRes;
	MpoUdpReceiver *pInstance = new MpoUdpReceiver(uPort);
	if (pInstance)
	{
		if (pInstance->Init())
		{
			pRes = MpoUdpReceiverSPtr(pInstance, MpoUdpReceiver::deleter());
		}
		else
		{
			delete pInstance;
		}
	}

	return pRes;
}

net_result MpoUdpReceiver::RecvPacket(string &strPacket, string &strSrcIP, unsigned int uTimeoutMs)
{
	net_result res = NET_ERROR;
	char buf[1024];	// this can be increased if we need to support large UDP packets (we shouldn't need to)
	sockaddr_in SenderAddr;
	int iSenderAddrSize = sizeof(SenderAddr);

	bool bCanRead = (better_select(m_iSocket, SELECT_READ, uTimeoutMs) > 0);

	// if there's no data to be read, return a timeout
	if (!bCanRead)
	{
		return NET_TIMEOUT;
	}

	int iBytesRead = recvfrom(m_iSocket,
		buf, 
		sizeof(buf), 
		0, 
		(sockaddr *)&SenderAddr, 
		(socklen_t *) &iSenderAddrSize);

	if (iBytesRead > 0)
	{
		strPacket = string(buf, iBytesRead);
		strSrcIP = GetIpFromSocket((sockaddr *) &SenderAddr);
		res = NET_OK;
	}
	else if (iBytesRead == 0)
	{
		res = NET_DISCONNECT;
	}
	// else error

	return res;
}

MpoUdpReceiver::MpoUdpReceiver(unsigned int uPort) :
m_uPort(uPort)
{
}

MpoUdpReceiver::~MpoUdpReceiver()
{
	Shutdown();
}

void MpoUdpReceiver::DeleteInstance()
{
	delete this;
}

bool MpoUdpReceiver::Init()
{
	bool bRes = false;
	m_iSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	m_addr.sin_family = AF_INET;
	m_addr.sin_port = htons(m_uPort);
	m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(m_addr.sin_zero,'\0',sizeof m_addr.sin_zero);

	if (bind(m_iSocket, (const sockaddr *) &m_addr, (int) sizeof(m_addr)) != -1)
	{
		bRes = true;
	}

	return bRes;
}

void MpoUdpReceiver::Shutdown()
{
	net_close(m_iSocket);
}

////////////////////////////////

IMpoUdpSPtr MpoUdp::GetInstance(unsigned int uSrcPort, bool bBroadcast)
{
	IMpoUdpSPtr pRes;
	MpoUdp *pInstance = new MpoUdp(uSrcPort, bBroadcast);
	if (pInstance)
	{
		try
		{
			pInstance->Init();
			pRes = IMpoUdpSPtr(pInstance, MpoUdp::deleter());
		}
		catch (std::exception &)
		{
			delete pInstance;
		}
	}

	return pRes;
}

void MpoUdp::SendPacketToEx(const string &strPacket, const string &strDstIP, unsigned int uDstPort)
{
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(uDstPort);
	addr.sin_addr.s_addr = inet_addr(strDstIP.c_str());
	memset(addr.sin_zero,'\0',sizeof(addr.sin_zero));

	int iNumBytes = sendto(m_iSocket, strPacket.data(), strPacket.size(), 0, (struct sockaddr *) &addr, sizeof(addr));
	if (iNumBytes < 0)
	{
		throw runtime_error(string("SendPacketTo failed: ") + numstr::ToStr(iNumBytes));
	}
}

net_result MpoUdp::RecvPacket(string *pstrPacket)
{
	net_result res = NET_ERROR;
	char buf[65535];	// this should be as big as the largest possible UDP packet
	sockaddr_in SenderAddr;
	int iSenderAddrSize = sizeof(SenderAddr);	// TODO : don't calculate this every time

	int iBytesRead = recvfrom(m_iSocket,
		buf, 
		sizeof(buf), 
		0, 
		(sockaddr *)&m_sendaddr, 
		(socklen_t *) &iSenderAddrSize);

	if (iBytesRead > 0)
	{
		*pstrPacket = string(buf, iBytesRead);
		res = NET_OK;
	}
	else if (iBytesRead == 0)
	{
		res = NET_DISCONNECT;
	}
	// else error

	return res;
}

MpoUdp::MpoUdp(unsigned int uSrcPort, bool bBroadcast)
{
}

MpoUdp::~MpoUdp()
{
	Shutdown();
}

void MpoUdp::DeleteInstance()
{
	delete this;
}

void MpoUdp::Init()
{
	if((m_iSocket = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1)
	{
		throw runtime_error("socket function failed");
	}

	if (m_bBroadcast)
	{
#ifdef WIN32
		char broadcast = 1;	// char for winsock compatibility
#else
		int broadcast = 1;
#endif
		if((setsockopt(m_iSocket,SOL_SOCKET,SO_BROADCAST,
			&broadcast,sizeof (broadcast))) == -1)
		{
			throw runtime_error("setsockopt function failed");
		}
	}

	m_sendaddr.sin_family = AF_INET;
	m_sendaddr.sin_port = htons(m_uSrcPort);
	m_sendaddr.sin_addr.s_addr = INADDR_ANY;
	memset(m_sendaddr.sin_zero,'\0',sizeof(m_sendaddr.sin_zero));
	
	if(bind(m_iSocket, (const sockaddr*) &m_sendaddr, (int) sizeof(m_sendaddr)) == -1)
	{
		throw runtime_error("Bind failed");
	}
}

void MpoUdp::Shutdown()
{
	net_close(m_iSocket);
}
////////////////////

IMpoUdpSPtr MpoUdpFactory::GetInstance(unsigned int uSrcPort, bool bBroadcast)
{
	return MpoUdp::GetInstance(uSrcPort, bBroadcast);
}
