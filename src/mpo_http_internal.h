//
// Created by Matt on 10/4/2019.
//

#ifndef MPO2_MPO_HTTP_INTERNAL_H
#define MPO2_MPO_HTTP_INTERNAL_H

#include <mpolib/mpo_net_stream.h>
#include <mpolib/mpo_deleter.h>

enum chunk_status
{
	CHUNK_SIZE,
	CHUNK_DATA,
	CHUNK_END
};

class mpo_http : public IMpoHttp, public MpoDeleter
{
public:
static IMpoHttpSPtr CreateInstance(INonblockingStream *pStream, const char *user_agent);

net_result StartGet(const string &http_host, const string &request_uri,
					unsigned int timeout_ms, const MapNameValue &mapOtherHeaders);

// using opened connection, sends an HTTP "GET" to begin a file download
// example of 'http_host' is "www.daphne-emu.com", exmaple of 'request_uri' is "/images/img.jpg"
// You can use net_parse_url to break a URL down into these components.
// if 'closecon' is true, the connection will close after having received the file
// NET_OK means that GetHeaders can now be called to stream in the file as it is downloaded
net_result StartGet(const string &http_host, const string &request_uri, bool closecon,
					unsigned int timeout_ms, const string &strLastModified = "", const string &strETag = "");

// using opened connection, sends an HTTP "HEAD" to remote server.
// example of 'http_host' is "www.daphne-emu.com", exmaple of 'request_uri' is "/images/img.jpg"
// You can use net_parse_url to break a URL down into these components.
// if 'closecon' is true, the connection will close after having received the response.
// NET_OK means that GetHeaders can now be called to get the response.
net_result StartHead(const string &http_host, const string &request_uri, bool closecon,
					 unsigned int timeout_ms, const string &strLastModified = "", const string &strETag = "");

net_result StartPost(const string &strHTTPHost, const string &strRequestURI, size_t stContentLength,
					 unsigned int uTimeoutMs, const MapNameValue &mapOtherHeaders);

// Sends HTTP "POST" to remote server.
// If NET_OK is returned, it means it's ok to call Send
net_result StartPost(const string &strHTTPHost, const string &strRequestURI, size_t stContentLength,
					 bool bCloseCon,
					 unsigned int uTimeoutMs, const string &strLastModified = "", const string &strETag = "");

// used if we are doing an HTTP POST. (sends the post data)
StreamMsg Send(const void *buffer, size_t bytes_can_send, size_t *bytes_sent, unsigned int timeout_ms);

// convenience buffer for sending small buffers as POST data (calls Send)
StreamMsg SendBuf(const string &buffer, unsigned int timeout_ms);

// This function receives headers from the server after StartGet has been called.
// Returns MSG_OK if all headers have been received and it's time to call Receive.
// Returns MSG_TIMEOUT if no data was received, or if some headers were received, but not all
StreamMsg GetHeaders(unsigned int timeout_ms);

// returns content length (if provided by headers) or 0 if unknown
size_t GetContentLength();

// returns Last Modified header or empty string if not available
string GetLastModified();

// returns ETag header or empty string if not evailable
string GetETag();

// returns m_http_result_code
unsigned int GetResultCode();

// This function streams in a downloaded file after HTTPStartGet has been called.
// MSG_OK means at least 1 byte was received
// MSG_END indicates transfer complete (useful if we don't know the size of the file we're downloading)
StreamMsg Receive(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms);

// Convenience function to download an entire file via HTTP
// returns MSG_END if 'buffer' is filled with complete file that we were receiving
StreamMsg FillBuf(string &buffer, unsigned int timeout_ms);

// escapes an arbitrary buffer so that it can be used in an HTTP URL
// (ie a CRLF combo would become "%0d%0a")
// See RFC 2396 to enhance this function
static string Escape(const void *buffer, size_t buf_size);

// creates a well-formed URI
static string MakeURI(const list<string> *pLstPathSegments, const string &strQuery = "");

private:
mpo_http();

// private so they can't delete the instance without going through shared_ptr's mechanism
~mpo_http();

void DeleteInstance();

// This is called from GetInstance
// 'user_agent' is the identifier string to send to the web server
//  (it basically consists of the name and version of the web browser)
void init(INonblockingStream *pStream, const char *user_agent);

net_result StartGetOrHead(const string &strGetOrHead, const string &http_host, const string &request_uri,
						  unsigned int timeout_ms, const MapNameValue &mapOtherHeaders);

net_result StartGetOrHead(const string &strGetOrHead, const string &http_host, const string &request_uri,
						  bool closecon, unsigned int timeout_ms, const string &strLastModified,
						  const string &strETag);

StreamMsg ReceiveChunked(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms);

StreamMsg ReceiveNormal(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms);

INonblockingStream *m_pStream;

// if we get messed up in our HTTP parsing, it's fatal, and we should not proceed ...
bool m_bFatalError;

// HTTP state

// our identifier
string m_user_agent;

// START HEADER
string m_partial_header;

// whether the headers were processed
bool m_headers_processed;

// returns NET_OK if header is ok, or NET_ERROR if there is a problem
// Does not disconnect on NET_ERROR because GetHeaders will do that
net_result ProcessHeader(const string &header);
// STOP HEADER

// START CHUNKED
bool m_http_chunked;	// whether HTTP encoding is chunked or not
chunk_status m_chunk_stat;
string m_chunk_size_line;	// string we receive to get the chunk size
size_t m_cur_chunk_size;	// size of current chunk
size_t m_cur_chunk_index;	// position we are inside of current chunk
// STOP CHUNKED

size_t m_http_content_length;	// total size of buffer we're receiving
size_t m_http_content_index;	// what position we're at in receiving from the buffer

size_t m_http_post_length;	// total size of POST buffer we may be sending
size_t m_http_post_index;	// what position we're at in sending from POST buffer

unsigned int m_http_result_code;	// the result code response from server

string m_httpStrLastModified;	// the last modified header (if available)

string m_httpStrETag;	// the ETag sent from the HTTP server (if any)
};

#endif //MPO2_MPO_HTTP_INTERNAL_H
