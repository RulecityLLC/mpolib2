#include <mpolib/mpo_nat_stream.h>
#include <stdexcept>
#include <assert.h>

IMpoNatStreamClientSPtr MpoNatStreamClient::GetInstance(IMpoUdpFactory *pUdpFactory, IMpoTimer *pTimer, string strRendezvousHost, unsigned int uRendezvousPort, unsigned int uSrcPort)
{
	IMpoNatStreamClientSPtr res;
	MpoNatStreamClient *pInstance = new MpoNatStreamClient(pUdpFactory, pTimer, strRendezvousHost, uRendezvousPort, uSrcPort);
	if (pInstance)
	{
		if (pInstance->Init())
		{
			res = IMpoNatStreamClientSPtr(pInstance, MpoNatStreamClient::deleter());
		}
		else
		{
			delete pInstance;
		}
	}
	return res;
}

unsigned int MpoNatStreamClient::StartConnectEx(const char *cpszDstHost, unsigned int uDstPort, unsigned int uRetryIntervalMs)
{
	unsigned int uRes;
	if (!StartConnect(&uRes, cpszDstHost, uDstPort, uRetryIntervalMs))
	{
		throw runtime_error(m_strLastErrMsg);
	}
	return uRes;
}

bool MpoNatStreamClient::StartConnect(unsigned int *puConnectionID, const char *cpszDstHost, unsigned int uDstPort, unsigned int uRetryIntervalMs)
{
	nat_connection_t con;
	string host_ip4;
	bool bRes = false;

	if (!net_nslookup(cpszDstHost, host_ip4))
	{
		m_strLastErrMsg = "Host lookup failed";
		return false;
	}

	con.m_uRetryIntervalMs = uRetryIntervalMs;
	con.m_uStartTimestamp = m_pTimer->GetCurValMs();
	con.m_uRetryTimestamp = 0;

	con.m_uDstPortInternal = uDstPort;
	con.m_strDstIP4 = host_ip4;

	// Prepare payload to be sent to rendezvous server
	// 4 bytes for version ID
	*((unsigned int *) con.m_arrPayload) = htonl(0x31333531);	// '1351' in ASCII
	// 4 bytes for local IPv4
	*((unsigned int *) (con.m_arrPayload + 4)) = 0;	// TODO : figure out a better representation of our local address
	// 2 bytes for local src port
	*((unsigned short *) (con.m_arrPayload + 8)) = htons(m_uSrcPort);
	// 4 bytes for target IPv4
	unsigned int uIPv4 = inet_addr(host_ip4.c_str());	// this returns the IP in network order already so no need to shuffle it around
	*((unsigned int *) (con.m_arrPayload + 10)) = uIPv4;
	// 2 bytes for target dst port
	*((unsigned short *) (con.m_arrPayload + 14)) = htons(uDstPort);
	// 4 bytes for connection ID
	*((unsigned int *) (con.m_arrPayload + 16)) = htonl(m_uConnectionIdx);
	
	m_mConnections[m_uConnectionIdx] = con;
	*puConnectionID = m_uConnectionIdx;
	m_uConnectionIdx++;

	return true;
}

net_result MpoNatStreamClient::CheckConnect(unsigned int uConnectionID)
{
	net_result res = NET_TIMEOUT;

	MapNatConnection::iterator mi = m_mConnections.find(uConnectionID);
	if (mi == m_mConnections.end())
	{
		return NET_ERROR;
	}

	ThinkRecv();	// handle any incoming packets
	Think(&mi->second);	// send outgoing packets if necessary

	if (mi->second.m_bConnected)
	{
		res = NET_OK;
	}

	return res;
}

nonblocking_sharedptr MpoNatStreamClient::GetStream(unsigned int uConnectionID)
{
	nonblocking_sharedptr res;

	throw runtime_error("Unfinished class");

	return res;
}

MpoNatStreamClient::MpoNatStreamClient(IMpoUdpFactory *pUdpFactory, IMpoTimer *pTimer, string strRendezvousHost, unsigned int uRendezvousPort, unsigned int uSrcPort) :
m_pUdpFactory(pUdpFactory),
m_pTimer(pTimer),
m_strRendezHost(strRendezvousHost),
m_uRendezPort(uRendezvousPort),
m_pUdp(NULL),
m_uSrcPort(uSrcPort),
m_bVersionMismatch(false)
{
	m_uConnectionIdx = 0;
}

void MpoNatStreamClient::DeleteInstance()
{
	delete this;
}

bool MpoNatStreamClient::Init()
{
	bool bRes = false;
	if (!net_nslookup(m_strRendezHost.c_str(), m_strRendezIPv4))
	{
		m_strLastErrMsg = "Unable to instantiate UDP instance";
		goto done;
	}

	m_udpSPtr = m_pUdpFactory->GetInstance(m_uSrcPort, false);
	m_pUdp = m_udpSPtr.get();

	if (!m_pUdp)
	{
		m_strLastErrMsg = "Unable to instantiate UDP instance";
		goto done;
	}

	bRes = true;

done:

	return bRes;
}

void MpoNatStreamClient::ThinkRecv()
{
	string strIncomingPacket;

	// see if anything has come in on our port
	net_result res = m_pUdp->RecvPacket(&strIncomingPacket);
	assert(res != NET_ERROR);

	// if we got something, then figure out what it is and whether it applies
	if (res == NET_OK)
	{
		const char *arr = strIncomingPacket.data();

		// check version
		unsigned int uVersion = ntohl(*((unsigned int *) arr));

		// if this version is unknown, then we can't understand its contents, so don't attempt to
		if (uVersion != 0x33333531)
		{
			m_bVersionMismatch = true;
			goto done;
		}
		
		// populate remainder of variables
		unsigned int uConnectionID = ntohl(*((unsigned int *) arr+4));
		unsigned int uTargetIPv4 = ntohl(*((unsigned int *) arr+8));
		unsigned int uTargetPort = ntohl(*((unsigned short *) arr+12));
		unsigned int uMsUntilNextSync = ntohl(*((unsigned int *) arr+14));
		unsigned int uMsUntilNextRendez = ntohl(*((unsigned int *) arr+18));

		MapNatConnection::iterator mi = m_mConnections.find(uConnectionID);

		// if we can't find this connection ID, then we don't know what to do with this packet, so ignore it
		if (mi == m_mConnections.end())
		{
			goto done;
		}

		nat_connection_t *tCon = &mi->second;
		tCon->m_uDstPortExternal = uTargetPort;
		tCon->m_stage = nat_connection_t::STAGE2_WAITING;
	}

done:
	return;
}

void MpoNatStreamClient::Think(nat_connection_t *pCon)
{
	switch (pCon->m_stage)
	{
			// if we haven't heard from the rendezvous server
	case nat_connection_t::STAGE0_INIT:
		// if it's too soon to send out any packets
		if (m_pTimer->GetElapsedMs(pCon->m_uRetryTimestamp) <= pCon->m_uRetryIntervalMs)
		{
			break;
		}

		// send another request since we're in the retry interval state
		m_pUdp->SendPacketToEx(string(pCon->m_arrPayload, sizeof(pCon->m_arrPayload)), m_strRendezIPv4, m_uRendezPort);
		pCon->m_uRetryTimestamp = m_pTimer->GetCurValMs();
		break;
	default:
		throw runtime_error("unimplemented");
		break;
	}

	return;
}
