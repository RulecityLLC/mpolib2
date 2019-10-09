#ifndef MPO_HTTPD_H
#define MPO_HTTPD_H

#include "mpo_stream.h"
#include <string>
#include <map>
using namespace std;

typedef map<string,string> MapNameValue;

class mpo_httpd_headers
{
public:
	mpo_httpd_headers(unsigned int uStatusCode, const string &strReasonPhrase) :
			u64ContentLength(0),
			m_uStatusCode(uStatusCode),
			m_strReasonPhrase(strReasonPhrase)
	{
	}

	bool operator== (const mpo_httpd_headers &h) const
	{
		return (h.m_uStatusCode == m_uStatusCode &&
				h.u64ContentLength == u64ContentLength &&
				h.m_strReasonPhrase == m_strReasonPhrase &&
				h.m_strContentType == m_strContentType &&
				h.m_mapOtherHeaders == m_mapOtherHeaders);
	}

	/////

	MPO_UINT64 u64ContentLength;
	unsigned int m_uStatusCode;
	string m_strReasonPhrase;
	string m_strContentType;
	MapNameValue m_mapOtherHeaders;
};

// the purpose of this interface is to make unit testing/mocking easier
class IMpoHttpd
{
public:
	// MSG_OK: method has been received, so it's ok to call ParseRequest
	// MSG_TIMEOUT: method has not been received fully (so call this function again until you get MSG_OK or MSG_ERROR)
	virtual StreamMsg ReadRequest(unsigned int uTimeoutMs) = 0;

	// parses the request read by ReadRequest.
	// Returns true on success, false on failure.
	// This function must be called after ReadRequest returns MSG_OK and before any other function is called.
	virtual bool ParseRequest() = 0;

	// returns for example "GET"
	virtual string GetMethod() const = 0;

	// returns for example "/whatever?query#fragment";
	// (this string will still be escaped!)
	virtual string GetURI() const = 0;

	// returns an unescaped string
	virtual string GetPath() const = 0;	// "/whatever" from "/whatever?query#fragment"
	virtual string GetQuery() const = 0;	// "query" from "/whatever?query#fragment"
	virtual string GetFragment() const = 0;	// "fragment" from "/whatever?query#fragment"

	// returns for example "HTTP/1.1"
	virtual string GetHTTPVersion() const = 0;

	// Gets client headers such as "Host"
	// Returns false if no such header was sent by the client.
	virtual bool GetHeader(string &strValue, const string &strName) = 0;

	// Reads in POST data.
	// Returns MSG_END if no more POST data is available (or no POST data was ever available).
	virtual StreamMsg Recv(void *buffer, size_t stBytesCanRead, size_t *pstBytesRead, unsigned int uTimeoutMs) = 0;

	// sets headers in preparation to call SendHeaders
	virtual bool SetHeaders(const mpo_httpd_headers *pHeaders) = 0;

	// This function sends headers to the client after GetMethod has been called.
	// Returns MSG_OK if all headers have been sent and it's time to call Send.
	// Returns MSG_TIMEOUT if not all headers have been sent (keep calling to send them).
	virtual StreamMsg SendHeaders(unsigned int uTimeoutMs) = 0;

	virtual StreamMsg Send(const void *buffer, size_t bytes_can_send, size_t *bytes_sent, unsigned int timeout_ms) = 0;

	// returns a string describing the last error that occurred (to help troubleshoot)
	virtual string GetLastErrorStr() = 0;
};

typedef shared_ptr<IMpoHttpd> IMpoHttpdSPtr;

class MpoHttpdFactory
{
public:
	static IMpoHttpdSPtr CreateInstance(INonblockingStream *pStream);
};

class mpo_httpd_listener;

typedef shared_ptr<mpo_httpd_listener> MpoHttpdListenerSPtr;


class IHttpdThreadComm
{
public:
	// Returns true if parent thread has requested that we quit.  (current thread should shutdown ASAP if this is true)
	virtual bool IsQuitRequested() = 0;

	// returns pointer to httpd interface that child thread must use
	virtual IMpoHttpd *GetHttpdInterface() = 0;

	// returns pointer to user-defined data, passed in via listener GetInstance call (can be NULL)
	virtual void *GetUserData() = 0;
};

// a class with no members; the client just keeps its shared pointer instantiated
class IMpoHttpdListener
{

};

typedef shared_ptr<IMpoHttpdListener> IMpoHttpdListenerSPtr;

class EXPORT_ME MpoHttpdListenerFactory
{
public:
	static IMpoHttpdListenerSPtr CreateInstance(unsigned int uListenPort, void *(*pCallback)(void*), void *data = NULL);
};

class EXPORT_ME MpoHttpdUtil
{
public:
	static string Unescape(const string &strEscaped);

	static void ParseURI(string &strDstPath, string &strDstQuery, string &strDstFragment, const string &strSrcURI);
};

#endif // MPO_HTTPD_H
