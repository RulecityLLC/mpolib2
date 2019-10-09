#ifndef MPO_NAT_STREAM_H
#define MPO_NAT_STREAM_H

#ifdef WIN32
#pragma warning (disable:4251)	// disable the warning about DLL interface
#endif

#include "mpo_timer.h"
#include "mpo_stream.h"
#include "mpo_deleter.h"
#include "mpo_udp.h"
#include <map>

using namespace std;

class nat_connection_t
{
public:
	nat_connection_t() :
	  m_stage(STAGE0_INIT),
	  m_uDstPortInternal(0),
	  m_uDstPortExternal(0),
	  m_bConnected(false),
	  m_uRetryIntervalMs(0),
	  m_uStartTimestamp(0),
	  m_uRetryTimestamp(0),
	  m_uSyncTimeMs(0)
	{}

	typedef enum
	{
		STAGE0_INIT,
		STAGE1_PINGING_RENDEZ,
		STAGE2_WAITING,
		STAGE3_PINGING_PEER,
		STAGE4_CONNECTED
	} stage_t;

	stage_t m_stage;
	unsigned int m_uDstPortInternal;
	unsigned int m_uDstPortExternal;
	string m_strDstIP4;
	bool m_bConnected;
	unsigned int m_uRetryIntervalMs;
	unsigned int m_uStartTimestamp;
	unsigned int m_uRetryTimestamp;
	unsigned int m_uSyncTimeMs;	// the time value when we try to sync to the remote peer (if we're at the proper stage)

	// All of this is big endian (network byte order)
	// 4 bytes for version ID
	// 4 bytes for local IPv4
	// 2 bytes for local src port
	// 4 bytes for target IPv4
	// 2 bytes for target dst port
	// 4 bytes for connection ID
	char m_arrPayload[20];

};

typedef map<unsigned int, nat_connection_t> MapNatConnection;

class IMpoNatStreamClient
{
public:
	virtual unsigned int StartConnectEx(const char *cpszDstHost, unsigned int uDstPort, unsigned int uRetryIntervalMs = 1000) = 0;

	virtual bool StartConnect(unsigned int *puConnectionID, const char *cpszDstHost, unsigned int uDstPort, unsigned int uRetryIntervalMs = 1000) = 0;

	// This function must be called to complete the connection process.
	// The results are either NET_OK, NET_ERROR, or NET_TIMEOUT
	virtual net_result CheckConnect(unsigned int uConnectionID) = 0;

	// Call this once the connection has been made to use the stream.
	virtual nonblocking_sharedptr GetStream(unsigned int uConnectionID) = 0;
};

typedef shared_ptr<IMpoNatStreamClient> IMpoNatStreamClientSPtr;

class EXPORT_ME MpoNatStreamClient : public IMpoNatStreamClient, public MpoDeleter
{
public:

	static IMpoNatStreamClientSPtr GetInstance(IMpoUdpFactory *pUdpFactory, IMpoTimer *pTimer, string strRendezvousHost, unsigned int uRendezvousPort, unsigned int uSrcPort);

	// Attempts to connect to 'host' (does DNS lookup if necessary) on the port indicated by 'port'.
	// A runtime_error exception is thrown on error.
	// Returns connection ID on success
	unsigned int StartConnectEx(const char *cpszDstHost, unsigned int uDstPort, unsigned int uRetryIntervalMs = 1000);

	bool StartConnect(unsigned int *puConnectionID, const char *cpszDstHost, unsigned int uDstPort, unsigned int uRetryIntervalMs = 1000);

	// This function must be called to complete the connection process.
	// The results are either NET_OK, NET_ERROR, or NET_TIMEOUT
	net_result CheckConnect(unsigned int uConnectionID);

	// Call this once the connection has been made to use the stream.
	nonblocking_sharedptr GetStream(unsigned int uConnectionID);
private:
	MpoNatStreamClient(IMpoUdpFactory *pUdpFactory, IMpoTimer *pTimer, string strRendezvousHost, unsigned int uRendezvousPort, unsigned int uSrcPort);

	void DeleteInstance();

	bool Init();

	void ThinkRecv();

	void Think(nat_connection_t *pConnection);

	/////////////////////////////////////
	
	// for functions that don't throw exceptions
	string m_strLastErrMsg;

	IMpoUdpFactory *m_pUdpFactory;
	IMpoTimer *m_pTimer;
	string m_strRendezHost;
	string m_strRendezIPv4;
	unsigned int m_uRendezPort;

	IMpoUdpSPtr m_udpSPtr;
	IMpoUdp *m_pUdp;

	unsigned int m_uSrcPort;
	
	// so that we always assign a unique connection ID
	unsigned int m_uConnectionIdx;

	MapNatConnection m_mConnections;

	// whether rendez server is an unknown/unsupported version (so we don't keep spamming it if it is)
	bool m_bVersionMismatch;
};


#endif // MPO_NAT_STREAM_H
