#ifndef MPO_HTTP_H
#define MPO_HTTP_H

#include <map>
#include <list>
#include "mpo_net.h"
#include "mpo_deleter.h"
#include "mpo_stream.h"
using namespace std;

typedef map<string,string> MapNameValue;

// interface for http client
class IMpoHttp
{
public:
	// Preferred version.  Headers that are sent automatically: Host, and User-Agent.  Any other headers must be added to 'mapOtherHeaders'
	virtual net_result StartGet(const string &http_host, const string &request_uri,
		unsigned int timeout_ms, const MapNameValue &mapOtherHeaders) = 0;

	// legacy version, kept for backward compatibility (also sends Cache-Control header)
	virtual net_result StartGet(const string &http_host, const string &request_uri, bool closecon,
		unsigned int timeout_ms, const string &strLastModified = "", const string &strETag = "") = 0;

	virtual net_result StartHead(const string &http_host, const string &request_uri, bool closecon,
		unsigned int timeout_ms, const string &strLastModified = "", const string &strETag = "") = 0;

	// Preferred version.  Headers that are sent automatically: Host, User-Agent, and Content-Length.  Any other headers must be added to 'mapOtherHeaders'
	virtual net_result StartPost(const string &strHTTPHost, const string &strRequestURI, size_t stContentLength,
		unsigned int uTimeoutMs, const MapNameValue &mapOtherHeaders) = 0;

	// legacy version, kept for backward compatibility (also sends Cache-Control header)
	virtual net_result StartPost(const string &strHTTPHost, const string &strRequestURI, size_t stContentLength,
		bool bCloseCon, unsigned int uTimeoutMs, const string &strLastModified = "", const string &strETag = "") = 0;

	virtual StreamMsg Send(const void *buffer, size_t bytes_can_send, size_t *bytes_sent, unsigned int timeout_ms) = 0;

	// returns MSG_END on success
	virtual StreamMsg SendBuf(const string &buffer, unsigned int timeout_ms) = 0;

	virtual StreamMsg GetHeaders(unsigned int timeout_ms) = 0;

	virtual size_t GetContentLength() = 0;

	virtual string GetLastModified() = 0;

	virtual string GetETag() = 0;

	virtual unsigned int GetResultCode() = 0;

	virtual StreamMsg Receive(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms) = 0;

	virtual StreamMsg FillBuf(string &buffer, unsigned int timeout_ms) = 0;

};

typedef shared_ptr <IMpoHttp> IMpoHttpSPtr;

class EXPORT_ME MpoHttpFactory
{
public:
	static IMpoHttpSPtr CreateInstance(INonblockingStream *pStream, const char *user_agent);
};

class EXPORT_ME MpoHttpUtil
{
public:
// escapes an arbitrary buffer so that it can be used in an HTTP URL
// (ie a CRLF combo would become "%0d%0a")
// See RFC 2396 to enhance this function
	static string Escape(const void *buffer, size_t buf_size);

// creates a well-formed URI
	static string MakeURI(const list<string> *pLstPathSegments, const string &strQuery = "");

};

#endif // MPO_HTTP_H
