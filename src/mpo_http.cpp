#include <mpolib/mpo_http.h>
#include <mpolib/mpo_misc.h>
#include <mpolib/mpo_numstr.h>
#include "mpo_http_internal.h"

#ifdef DEBUG
#include <iostream>
#endif // DEBUG

IMpoHttpSPtr MpoHttpFactory::CreateInstance(INonblockingStream *pStream, const char *user_agent)
{
	return mpo_http::CreateInstance(pStream, user_agent);
}

IMpoHttpSPtr mpo_http::CreateInstance(INonblockingStream *pStream, const char *user_agent)
{
	mpo_http *pInstance = new mpo_http();
	pInstance->init(pStream, user_agent);
	return IMpoHttpSPtr(pInstance, mpo_http::deleter());
}

mpo_http::mpo_http() :
m_bFatalError(false)
{
	// init should always be called before anything else...
	m_user_agent = "";
	m_partial_header = "";
	m_chunk_size_line = "";
}

mpo_http::~mpo_http()
{
}

void mpo_http::DeleteInstance()
{
	delete this;
}

void mpo_http::init(INonblockingStream *pStream, const char *user_agent)
{
	m_pStream = pStream;
	m_partial_header = "";
	m_headers_processed = false;
	m_http_chunked = false;
	m_cur_chunk_size = m_cur_chunk_index = 0;
	m_http_content_length = m_http_content_index = m_http_result_code = 0;
	m_http_post_length = m_http_post_index = 0;
	m_chunk_stat = CHUNK_SIZE;
	m_chunk_size_line = "";
	m_user_agent = user_agent;
}

net_result mpo_http::StartGet(const string &http_host, const string &request_uri,
		unsigned int timeout_ms, const MapNameValue &mapOtherHeaders)
{
	m_http_post_length = 0;	// don't send content-length
	return StartGetOrHead("GET", http_host, request_uri, timeout_ms, mapOtherHeaders);
}


net_result mpo_http::StartGet(const string &http_host, const string &request_uri,
									bool closecon, unsigned int timeout_ms, const string &strLastModified,
								const string &strETag)
{
	m_http_post_length = 0;	// don't send content-length
	return StartGetOrHead("GET", http_host, request_uri, closecon, timeout_ms, strLastModified, strETag);
}

net_result mpo_http::StartHead(const string &http_host, const string &request_uri,
									bool closecon, unsigned int timeout_ms, const string &strLastModified,
								const string &strETag)
{
	m_http_post_length = 0;	// don't send content-length
	return StartGetOrHead("HEAD", http_host, request_uri, closecon, timeout_ms, strLastModified, strETag);
}

net_result mpo_http::StartPost(const string &strHTTPHost, const string &strRequestURI, size_t stContentLength,
	unsigned int uTimeoutMs, const MapNameValue &mapOtherHeaders)
{
	m_http_post_length = stContentLength;
	m_http_post_index = 0;
	return StartGetOrHead("POST", strHTTPHost, strRequestURI, uTimeoutMs, mapOtherHeaders);
}

net_result mpo_http::StartPost(const string &strHTTPHost, const string &strRequestURI, size_t stContentLength,
		bool bCloseCon,
		unsigned int uTimeoutMs, const string &strLastModified, const string &strETag)
{
	m_http_post_length = stContentLength;
	m_http_post_index = 0;
	return StartGetOrHead("POST", strHTTPHost, strRequestURI, bCloseCon, uTimeoutMs, strLastModified, strETag);
}

StreamMsg mpo_http::Send(const void *buffer, size_t bytes_can_send, size_t *bytes_sent, unsigned int timeout_ms)
{
	StreamMsg res = MSG_END;

	// if there is more post data left to be sent
	if (m_http_post_index < m_http_post_length)
	{
		size_t stDataLeft = m_http_post_length - m_http_post_index;

		if (bytes_can_send > stDataLeft)
		{
			bytes_can_send = stDataLeft;
		}

		size_t stBytesSent = m_pStream->Write(buffer, bytes_can_send, timeout_ms);
		res = m_pStream->GetLastMsg();

		// if bytes were sent successfully
		if (res == MSG_OK)
		{
			if (bytes_sent != NULL)
			{
				*bytes_sent = stBytesSent;
			}

			m_http_post_index += stBytesSent;

			// By arbitrary definition (and to be consistent with Receive), MSG_END does not return ANY data, so we should return MSG_OK here even if
			//  we're done
		}
		// else some other result, which we do not want to modify because it is applicable to our context as well
	}
	// else all post data has been sent

	return res;
}

StreamMsg mpo_http::SendBuf(const string &buffer, unsigned int timeout_ms)
{
	StreamMsg msg = MSG_ERROR;
	size_t stBytesSent = 0, stBytesLeft = buffer.size();
	const char *p8Buf = buffer.data();

	// go until we can't send anymore
	for (;;)
	{
		msg = Send(p8Buf, stBytesLeft, &stBytesSent, timeout_ms);

		// by our definition, MSG_END does _not_ send any data.
		if (msg == MSG_OK)
		{
			stBytesLeft -= stBytesSent;
			p8Buf += stBytesSent;
		}

		// MSG_END is proper way to exit, but we'll exit on an error too
		else
		{
			break;
		}
	}

	return msg;

}

net_result mpo_http::StartGetOrHead(const string &strGetOrHead, const string &http_host, const string &request_uri,
	unsigned int timeout_ms, const MapNameValue &mapOtherHeaders)
{
	net_result result = NET_OK;	// FIXME : check for send errors
	string s;

	// initialize
	m_http_content_length = 0;
	m_http_content_index = 0;
	m_http_chunked = false;
	m_http_result_code = 0;
	m_httpStrLastModified.clear();
	m_httpStrETag.clear();

	s = strGetOrHead;
	s += " ";
	s += request_uri;	// NOTE : this should already be escaped
	s += " HTTP/1.1";
	StreamFull::WriteCRLF(m_pStream, s.data(), (int) s.size(), timeout_ms);
	s = "Host: ";
	s += http_host;
	StreamFull::WriteCRLF(m_pStream, s.data(), (int) s.size(), timeout_ms);
	s = "User-Agent: ";
	s += m_user_agent;
	StreamFull::WriteCRLF(m_pStream, s.data(), (int) s.size(), timeout_ms);

	// if there is a POST length, it means this is a POST request
	if (m_http_post_length != 0)
	{
		s = "Content-Length: ";
		s += numstr::ToStr(m_http_post_length);
		StreamFull::WriteCRLF(m_pStream, s.data(), s.size(), timeout_ms);
	}

	// send the other headers
	for (MapNameValue::const_iterator mi = mapOtherHeaders.begin(); mi != mapOtherHeaders.end(); mi++)
	{
		s = mi->first;
		s += ": ";
		s += mi->second;
		StreamFull::WriteCRLF(m_pStream, s.data(), s.size(), timeout_ms);
	}

	StreamFull::WriteCRLF(m_pStream, NULL, 0, timeout_ms);	// blank line to complete the request

	m_partial_header = "";	// clear ...
	m_headers_processed = false;

	return result;
}

net_result mpo_http::StartGetOrHead(const string &strGetOrHead, const string &http_host, const string &request_uri,
									bool closecon, unsigned int timeout_ms, const string &strLastModified, const string &strETag)
{
	// DEPRECATED FUNCTION

	MapNameValue headers;

	// if they've provided a last modified date
	if (!strLastModified.empty())
	{
		headers["If-Modified-Since"] = strLastModified;
	}

	// if they've provided an ETag
	if (!strETag.empty())
	{
		headers["If-None-Match"] = strETag;
	}

	// legacy compatibility
	headers["Cache-Control"] = "no-cache";

	if (closecon)
	{
		headers["Connection"] = "close";
	}

	return StartGetOrHead(strGetOrHead, http_host, request_uri, timeout_ms, headers);
}

StreamMsg mpo_http::GetHeaders(unsigned int timeout_ms)
{
	unsigned char ch = 0;
	size_t bytes_read = 0;
	StreamMsg msg = MSG_OK;

	// read as much as we can
	for (;;)
	{
		// read 1 character
		bytes_read = m_pStream->Read(&ch, 1, timeout_ms);
		msg = m_pStream->GetLastMsg();

		// if we successfully read the header
		if (bytes_read == 1)
		{
			// ignore CR's (to be more compatible with broken web servers that only send LF's)
			if (ch == 13) continue;

			// linefeed indicates end of string
			else if (ch == 10)
			{
				// if this isn't the end ...
				if (!m_partial_header.empty())
				{
					net_result res = ProcessHeader(m_partial_header);
					if (res != NET_OK)
					{
						msg = MSG_ERROR;
						break;
					}
					m_partial_header = "";	// clear it ...
				}
				// if we're done receiving headers ...
				else
				{
					// by breaking here, we're return NET_OK which is what we want to do
					m_headers_processed = true;
					break;
				}
			}

			// else add the character to our partial line
			else m_partial_header += ch;
		}
		// else it's a timeout or an error, which we will not tolerate in this situation
		else
		{
			break;
		}
	}

	// we cannot afford to have any tolerance for errors during the HTTP header phase
	if ((msg != MSG_TIMEOUT) && (msg != MSG_OK))
	{
		m_bFatalError = true;
	}

	return msg;
}

net_result mpo_http::ProcessHeader(const string &s)
{
	net_result res = NET_OK;
	
	// if it's the HTTP header with the response code (we'll allow 1.1 or 1.0)
	if (mpom::str_toupper(s).substr(0, 7) == "HTTP/1.")
	{
		m_http_result_code = numstr::ToInt32(s.substr(8, 4).c_str());
		
		// UPDATE:
		// Even if the web server returns an error like 404, we should still return OK because we need
		//  to be able to make another request after receiving the error.
	}
				
	// check to see if we're doing 'chunked' transfer encoding
	else if (mpom::str_toupper(s).substr(0, 17) == "TRANSFER-ENCODING")
	{
		string encoding_type = s.substr(19);
		mpom::trim(encoding_type);	// get rid of whitespace
		
		// if this is 'chunked' encoding
		if (mpom::str_toupper(encoding_type) == "CHUNKED")
		{
			m_http_chunked = true;
			m_cur_chunk_size = 0;	// initialize
			m_cur_chunk_index = 0;	// " " "
			m_chunk_stat = CHUNK_SIZE;	// " " "
			m_chunk_size_line = "";	// " " "
		}
		// else if this is something we don't know how to handle
		else
		{
			res = NET_ERROR;
		}
	}
				
	// we require content length to be present ...
	else if (mpom::str_toupper(s).substr(0, 14) == "CONTENT-LENGTH")
	{
		string test = s.substr(16);
		m_http_content_length = numstr::ToUint32(s.substr(16).c_str());	// convert content length into integer
	}
				
	// if this isn't 'normal' encoding
	else if (mpom::str_toupper(s).substr(0, 16) == "CONTENT-ENCODING")
	{
		res = NET_ERROR;	// we don't support content encoding at all!!!
	}

	// if we've got the time last modified
	else if (mpom::str_toupper(s).substr(0, 13) == "LAST-MODIFIED")
	{
		m_httpStrLastModified = s.substr(15);	// grab the last modified string
	}

	else if (mpom::str_toupper(s).substr(0, 4) == "ETAG")
	{
		m_httpStrETag = s.substr(6);	// grab the ETag string
	}

	// else ignore

	return res;
}

size_t mpo_http::GetContentLength()
{
#ifdef DEBUG
	// they shouldn't be querying content length without processing the headers first
	assert(m_headers_processed);
#endif

	return m_http_content_length;
}

string mpo_http::GetLastModified()
{
#ifdef DEBUG
	assert(m_headers_processed);
#endif
	return m_httpStrLastModified;
}

string mpo_http::GetETag()
{
#ifdef DEBUG
	assert(m_headers_processed);
#endif
	return m_httpStrETag;
}

unsigned int mpo_http::GetResultCode()
{
#ifdef DEBUG
	assert(m_headers_processed);
#endif
	return m_http_result_code;
}

StreamMsg mpo_http::Receive(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms)
{
	size_t dummy_bytes_read = 0;
	StreamMsg msg = MSG_ERROR;

#ifdef DEBUG
	// make sure GetHeaders was called to exhaustion
	assert(m_headers_processed);
#endif

	if (m_bFatalError)
	{
		return MSG_ERROR;
	}

	// if they don't care how many bytes were read
	if (bytes_read == NULL) bytes_read = &dummy_bytes_read;

	*bytes_read = 0;	// initialize this since it may not ever get set anywhere else in this function

	if (m_http_chunked)
	{
		msg = ReceiveChunked(buffer, bytes_can_read, bytes_read, timeout_ms);
	}
	// else no encoding, so it's just a straight read
	else
	{
		msg = ReceiveNormal(buffer, bytes_can_read, bytes_read, timeout_ms);
	}
	
	// if we get any kind of error, the parsing is totally out of sync,
	// so we need to stop
	if (msg == MSG_ERROR)
	{
		m_bFatalError = true;
	}

	return msg;
}

StreamMsg mpo_http::ReceiveChunked(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms)
{
	StreamMsg msg = MSG_OK;
	unsigned char *pu8Buf = (unsigned char *) buffer;

	// try to fill the buffer (for efficiency reasons)
	while ((msg == MSG_OK) && (*bytes_read < bytes_can_read))
	{
		// if we're waiting for chunk size and CRLF
		// then read line until CRLF
		if (m_chunk_stat == CHUNK_SIZE)
		{
			char ch = 0;

			// read 1 byte
			size_t stCurBytesRead = m_pStream->Read(&ch, 1, timeout_ms);

			// if we got something ...
			if (stCurBytesRead == 1)
			{
				// if this is end of the chunk size line
				if (ch == 10)
				{
					m_cur_chunk_index = 0;
					m_cur_chunk_size = numstr::ToUint32(m_chunk_size_line.c_str(), 16);	// convert hex to uint to get chunk size
					m_chunk_stat = CHUNK_DATA;	// ready to read data ...
				}
				else if (ch == 13)
				{
					// we can safely ignore CR's ....
				}
				else
				{
					m_chunk_size_line += ch;
				}
			}
			// else if it's not a timeout
			else if (m_pStream->GetLastMsg() != MSG_TIMEOUT)
			{
#ifdef DEBUG
				cout << "http::Receive error: unable to get chunk size!" << endl;
#endif
				msg = MSG_ERROR;	// something went wrong, bail out
			}
			// else we timed out, so no error ...
		}

		// NOTE!! this is not an else/if, it is just an if, because we want to flow into this
		// if we started as a CHUNK_SIZE
		if (m_chunk_stat == CHUNK_DATA)
		{
			if (m_cur_chunk_size > 0)
			{
				size_t stBytesToRead = bytes_can_read - m_cur_chunk_index;
				size_t bytes_remaining = m_cur_chunk_size - m_cur_chunk_index;	// how many bytes left in this chunk

				// make sure we don't read more than we have left to read
				if (stBytesToRead > bytes_remaining) stBytesToRead = bytes_remaining;

				size_t stBytesRead = m_pStream->Read(pu8Buf, stBytesToRead, timeout_ms);
				msg = m_pStream->GetLastMsg();

				// try to receive some of the chunk
				if (msg == MSG_OK)
				{
					pu8Buf += stBytesRead;
					m_cur_chunk_index += stBytesRead;
					*bytes_read = *bytes_read + stBytesRead;

					// if we finished reading our chunk, then move us into the next phase
					if (m_cur_chunk_index == bytes_remaining)
					{
						string trailingCRLF = "";

						// read trailing CRLF
						if (StreamFull::ReadUntilCRLF(m_pStream, trailingCRLF, timeout_ms) == MSG_OK)
						{
							// this _should_ be empty ...
							if (trailingCRLF.empty())
							{
								m_chunk_stat = CHUNK_SIZE;
								m_chunk_size_line = "";	// reset
							}
							// somthing went wrong with the parsing
							else
							{
#ifdef DEBUG
								cout << "http::Receive Expected just CRLF, but got " << trailingCRLF << endl;
#endif
								msg = MSG_ERROR;
							}
						}
						// unknown error, bail
						else
						{
#ifdef DEBUG
							cout << "http::Receive error, didn't get trailing CRLF after chunk-data!" << endl;
#endif
							msg = MSG_ERROR;
						}
					}
					// else we've got more yet to read
				}
				// something went wrong, we may write more comprehensive handling later, but for now bail out
				else if (msg != MSG_TIMEOUT)
				{
#ifdef DEBUG
					cout << "http::Receive, something went wrong when trying to read chunk-data" << endl;
#endif
					msg = MSG_ERROR;
				}
				// else it's a timeout so we need not take any action
			} // end if chunk size is > 0

			// chunk size is equal to 0, so we're done with the chunk!! :)
			else
			{
				m_chunk_stat = CHUNK_END;

				// since we are returning _some_ data, we need to return MSG_OK because MSG_END means no data is returned.
				msg = MSG_OK;

				string trailing_buf = "";

				// flush any HTTP trailer that may be there ...
				// (we'll know skipped all the trailers once we get a CRLF on a line by itself)
				while (StreamFull::ReadUntilCRLF(m_pStream, trailing_buf, timeout_ms) == MSG_OK)
				{
					// if we get the CRLF on the line by itself
					if (trailing_buf.size() == 0)
					{
						break;
					}
				}

				// the while loop will want to keep going but we need to return at this point to pass our unit tests :)
				break;

			}
		} // end if we're doing chunk data

		else if (m_chunk_stat == CHUNK_END)
		{
			msg = MSG_END;
			m_http_chunked = false;
		}

		// else we're CHUNK_SIZE apparently ...

	} // end while

	return msg;
}

StreamMsg mpo_http::ReceiveNormal(void *buffer, size_t bytes_can_read, size_t *bytes_read, unsigned int timeout_ms)
{
	StreamMsg msg = MSG_ERROR;

	// if the server has generously given us a content length
	// (it isn't required to according to HTTP 1.1 RFC, section 4.4)
	if (m_http_content_length > 0)
	{
		// if we still have bytes left to read
		if (m_http_content_index < m_http_content_length)
		{
			size_t bytes_remaining = m_http_content_length - m_http_content_index;

			// don't overflow
			if (bytes_can_read > bytes_remaining) bytes_can_read = bytes_remaining;

			*bytes_read = m_pStream->Read(buffer, bytes_can_read, timeout_ms);

			msg = m_pStream->GetLastMsg();
			if (msg == MSG_OK)
			{
				m_http_content_index += *bytes_read;
			}
			// make disconnects and errors count the same
			else if (msg != MSG_TIMEOUT)
			{
				msg = MSG_ERROR;
			}
			// else it was a timeout, so no action is required
		}

		// we've got nothing left to read!
		else
		{
			msg = MSG_END;
		}
	}
	// else we have no content length, so the end of the content is when the server
	// severs connection with us
	else
	{
		*bytes_read = m_pStream->Read(buffer, bytes_can_read, timeout_ms);
		msg = m_pStream->GetLastMsg();

		// we have to assume that the server disconnected from us if we don't
		// get an OK or a timeout
		if ((msg != MSG_OK) && (msg != MSG_TIMEOUT))
		{
			msg = MSG_END;
		}
		// else return the OK or TIMEOUT
	}

	return msg;
}

StreamMsg mpo_http::FillBuf(string &buffer, unsigned int timeout_ms)
{
	StreamMsg msg = MSG_ERROR;
	size_t cur_bytes_read = 0;

	unsigned char minibuf[1024];
	buffer = "";

	// go until we can't read anymore (NET_EOF is the proper termination result)
	for (;;)
	{
		msg = Receive(minibuf, sizeof(minibuf), &cur_bytes_read, timeout_ms);

		// by our definition, MSG_END does _not_ return any data.
		if (msg == MSG_OK)
		{
			// sometimes Receive can return 0 bytes read (if it is chunked)
			if (cur_bytes_read > 0)
			{
				buffer.append((const char *) minibuf, cur_bytes_read);
			}
		}

		// MSG_END is proper way to exit, but we'll exit on an error too
		if (msg != MSG_OK)
		{
			break;
		}
	}

	return msg;
}

string MpoHttpUtil::Escape(const void *buffer, size_t buf_size)
{
	string result = "";

	for (unsigned int i = 0; i < buf_size; i++)
	{
		unsigned char ch = *((unsigned char *) buffer + i);

		// if character is alphanumeric, then pass it through
		// TODO : add the other 'unreserved' characters from RFC2396, section 2.3
		if (isalpha(ch) || isdigit(ch))
		{
			result += ch;
		}
		else
		{
			result += '%';
			result += mpom::bin2hex((const unsigned char *) &ch, 1);
		}
	}

	return result;
}

string MpoHttpUtil::MakeURI(const list<string> *pLstPathSegments, const string &strQuery)
{
	string strRes;

	if (pLstPathSegments != NULL)
	{
		for (list<string>::const_iterator li = pLstPathSegments->begin();
			li != pLstPathSegments->end();
			li++)
		{
			strRes += "/";
			strRes += Escape(li->data(), li->size());
		}

		// if query is included
		if (!strQuery.empty())
		{
			strRes += "?";
			strRes += strQuery;
		}
	}

	return strRes;
}
