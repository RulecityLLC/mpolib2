//
// Created by Matt on 10/4/2019.
//

#ifndef MPO2_MPO_HTTPD_INTERNAL_H
#define MPO2_MPO_HTTPD_INTERNAL_H

#include <mpolib/mpo_httpd.h>
#include <mpolib/mpo_deleter.h>
#include <mpolib/mpo_stream.h>
#include <mpolib/mpo_numstr.h>	// for 64-bit number
#include <mpolib/mpo_server.h>
#include <mpolib/mpo_thread.h>

class mpo_httpd : public IMpoHttpd, public MpoDeleter
{
public:
static IMpoHttpdSPtr CreateInstance(INonblockingStream *pStream);

// MSG_OK: method has been received, so it's ok to call ParseRequest
// MSG_TIMEOUT: method has not been received fully (so call this function again until you get MSG_OK or MSG_ERROR)
StreamMsg ReadRequest(unsigned int uTimeoutMs);

// parses the request read by ReadRequest.
// Returns true on success, false on failure.
// This function must be called after ReadRequest returns MSG_OK and before any other function is called.
bool ParseRequest();

// returns for example "GET"
string GetMethod() const;

// returns for example "/whatever?query#fragment";
// (this string will still be escaped!)
string GetURI() const;

// returns an unescaped string
string GetPath() const;	// "/whatever" from "/whatever?query#fragment"
string GetQuery() const;	// "query" from "/whatever?query#fragment"
string GetFragment() const;	// "fragment" from "/whatever?query#fragment"

// returns for example "HTTP/1.1"
string GetHTTPVersion() const;

// Gets client headers such as "Host"
// Returns false if no such header was sent by the client.
bool GetHeader(string &strValue, const string &strName);

// Reads in POST data.
// Returns MSG_END if no more POST data is available (or no POST data was ever available).
StreamMsg Recv(void *buffer, size_t stBytesCanRead, size_t *pstBytesRead, unsigned int uTimeoutMs);

// sets headers in preparation to call SendHeaders
bool SetHeaders(const mpo_httpd_headers *pHeaders);

// This function sends headers to the client after GetMethod has been called.
// Returns MSG_OK if all headers have been sent and it's time to call Send.
// Returns MSG_TIMEOUT if not all headers have been sent (keep calling to send them).
StreamMsg SendHeaders(unsigned int uTimeoutMs);

StreamMsg Send(const void *buffer, size_t bytes_can_send, size_t *bytes_sent, unsigned int timeout_ms);

string GetLastErrorStr();

static string Unescape(const string &strEscaped);

// if 'strSrcURI' is "/a/b/c?d#e" then
//  'strDstPath' is "/a/b/c", 'strDstQuery' is "d" and 'strDstFragment' is "e"
static bool ParseURI(string &strDstPath, string &strDstQuery, string &strDstFragment, const string &strSrcURI);

private:
typedef enum
{
	RDY4_REQUEST,
	RDY4_PARSE,
	RDY4_RECV_POST,	// optional, only happens if method is POST
	RDY4_SET_HEADER,
	RDY4_SEND_HEADER,
	RDY4_SEND_BODY,
	RDY4_DISCONNECT	// if we client requested that this connection be closed
} HttpdStage;

// internal function
StreamMsg ReadPartialRequest(unsigned int uTimeoutMs);

	mpo_httpd();
	virtual ~mpo_httpd();

	void DeleteInstance() { delete this; }

// This is called from GetInstance
bool Init(INonblockingStream *pStream);

// helper function that streams in bytes and adds them to m_strCurBuf for parsing
StreamMsg ReadIntoBuf(unsigned int uTimeoutMs);

// TODO : move these into a general text parse class
bool GetNextLine(string &strDst, string &strSrc);
bool GetNextWord(string &strDst, string &strSrc);

/////////////////////////////////////////////

INonblockingStream *m_pStream;

HttpdStage m_stage;

// general purpose input buffer to deal with streaming
string m_strCurBuf;

// general purpose output buffer
string m_strCurOutBuf;

// the full request from the client
string m_strRequest;

// from the request line
string m_strMethod;
string m_strURI;
string m_strURIPath, m_strURIQuery, m_strURIFragment;	// parsed and escaped from m_strURI
string m_strHTTPVersion;

// to store various client headers
MapNameValue m_mapNameValuePairs;

const mpo_httpd_headers *m_pHeaders;

// so we know when we've sent the entire content length
MPO_UINT64 m_u64BodyBytesSent;

// so we know when we've read the entire POST body
MPO_UINT64 m_u64PostBytesRead;
MPO_UINT64 m_u64PostBytes;	// total # of bytes in POST body

// last error message that occurred
string m_strLastErrorStr;
};

class CListenerComm
{
public:
	CListenerComm() :
			pInstance(NULL),
			m_bParentRequestedQuit(false),
			m_bChildHasQuit(false)
	{
	}

	// So listener thread can use all of the instance's private variables.
	// Since mpo_httpd_listener doesn't have any public non-static methods, it is safe for the listener thread to do this without using mutex protection.
	mpo_httpd_listener *pInstance;

	bool m_bParentRequestedQuit;	// set by parent thread when quit is requested
	bool m_bChildHasQuit;	// set by child thread when it has quit
};

class CHttpdThreadComm;

typedef shared_ptr<CHttpdThreadComm> CHttpdThreadCommSPtr;

class CHttpdThreadComm : public IHttpdThreadComm, public MpoDeleter
{
	friend class mpo_httpd_listener;	// so that the httpd_listener thread helper can access private member vars and so child thread can't mess with them

public:
	// The reason I made this so it must be instantiated via CreateInstance is to protect syncronization between child and parent for the member variables that this class contains.
	// Having it instantiated one time and passed around as a shared pointer does this.  Using the class's copy constructor does NOT.
	static CHttpdThreadCommSPtr CreateInstance()
	{
		return CHttpdThreadCommSPtr(new CHttpdThreadComm(), CHttpdThreadComm::deleter());
	}

	// Returns true if parent thread has requested that we quit.  (current thread should shutdown ASAP if this is true)
	bool IsQuitRequested() { return m_bParentRequestedQuit; }

	IMpoHttpd *GetHttpdInterface() { return pHTTPD; }

	void *GetUserData() { return data; }

private:
	CHttpdThreadComm() :
			m_pCallback(NULL),
			m_bChildHasQuit(false),
			m_bParentRequestedQuit(false)
	{
	}

	virtual ~CHttpdThreadComm() {}

	void DeleteInstance() { delete this; }

	IMpoHttpd *pHTTPD;	// pointer to httpd instance that child thread must use
	void *data;	// pointer to user-defined data, passed in via listener GetInstance call (can be NULL)

	void *(*m_pCallback)(void*);	// so that helper callback knows which callback to call
	bool m_bChildHasQuit;	// set by child thread when it has quit
	bool m_bParentRequestedQuit;	// set by parent thread when quit is requested
};

class mpo_httpd_listener : public IMpoHttpdListener, public MpoDeleter
{
public:
	static IMpoHttpdListenerSPtr CreateInstance(unsigned int uListenPort, void *(*pCallback)(void*), void *data = NULL);

private:
	mpo_httpd_listener();
	virtual ~mpo_httpd_listener();

	void DeleteInstance() { delete this; }

	// This thread runs until the thread instance tells it to exit by setting a quit flag
	static void *ListenerThread(void *pListenerComm);

	static void *ListenerThreadHelper(void *pHttpThreadComm);

	///////////////////////////////////

	void *(*m_pCallback)(void *);	// callback called when new HTTPD connection is established
	void *data;	// user-defined data passed to callback

	// to accept connections
	IMpoServerSPtr m_srv;
	IMpoServer *m_pServer;

	// to communicate with child thread
	CListenerComm m_comm;

	// thread ID of listener thread
	mpo_threadID m_listenerID;
};

// so that two threads aren't sharing shared-pointers (which aren't thread safe in this case)
class CHttpdListenerStorage
{
public:
	CHttpdListenerStorage() {}

	CHttpdThreadCommSPtr m_threadCommSPtr;
	CHttpdThreadComm *m_pThreadComm;	// for speed
	nonblocking_sharedptr m_stream;
	IMpoHttpdSPtr m_httpdSPtr;
};

#endif //MPO2_MPO_HTTPD_INTERNAL_H
