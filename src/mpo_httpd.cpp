#include <mpolib/mpo_httpd.h>
#include <mpolib/mpo_timer.h>
#include <mpolib/mpo_misc.h>
#include <mpolib/mpo_net_stream.h>
#include "mpo_httpd_internal.h"
#include <string.h>	// for memcpy
#include <stdexcept>	// for runtime_error

IMpoHttpdSPtr MpoHttpdFactory::CreateInstance(INonblockingStream *pStream)
{
	return mpo_httpd::CreateInstance(pStream);
}

IMpoHttpdSPtr mpo_httpd::CreateInstance(INonblockingStream *pStream)
{
	IMpoHttpdSPtr pRes;
	mpo_httpd *pInstance = new mpo_httpd();
	if (pInstance)
	{
		if (pInstance->Init(pStream))
		{
			pRes = IMpoHttpdSPtr(pInstance, mpo_httpd::deleter());
		}
		else
		{
			delete pInstance;
			throw runtime_error("httpd init failed");
		}
	}
	return pRes;
}

StreamMsg mpo_httpd::ReadPartialRequest(unsigned int uTimeoutMs)
{
	StreamMsg res = MSG_ERROR;

	res = ReadIntoBuf(uTimeoutMs);

	// if we got something, check to see if we've received a full request
	if (res == MSG_OK)
	{
		char CRLFCRLF[4] = { 0xD, 0xA, 0xD, 0xA };
		string strCRLFCRLF = string(CRLFCRLF, sizeof(CRLFCRLF));
//		char LFLF[2] = { 0xA, 0xA };
//		string strLFLF = string(LFLF, sizeof(LFLF));

		size_t stPos = m_strCurBuf.find(strCRLFCRLF);

		// if we've read the full request
		if (stPos != string::npos)
		{
			m_strRequest = m_strCurBuf.substr(0, stPos);

			// Remove request from cur buf so we can keep using cur buf for later streaming
			// Add 4 to account for the trailing CRLFCRLF which we also want to remove.
			m_strCurBuf = m_strCurBuf.erase(0, stPos + 4);

			// res is MSG_OK which is what we want to return
		}
		// else we haven't received a full request, so return a TIMEOUT so we get called again by the sender
		else
		{
			res = MSG_TIMEOUT;
		}
	}

	return res;
}

StreamMsg mpo_httpd::ReadRequest(unsigned int uTimeoutMs)
{
	unsigned int uStartTime = MpoTimerUtil::RefreshTimer();
	StreamMsg msg = MSG_ERROR;

	// if the client has disconnected (or should be disconnected due to requesting a disconnect)
	if (m_stage == RDY4_DISCONNECT)
	{
		return MSG_END;
	}

	// if they call this at the wrong time, then error out immediately
	if (m_stage != RDY4_REQUEST)
	{
		m_strLastErrorStr = "ReadRequest function call came at an inappropriate time";
		return MSG_ERROR;
	}

	// go until we time out or fail
	for (;;)
	{
		unsigned int uElapsed = MpoTimerUtil::GetElapsedMs(uStartTime);
		if (uElapsed > uTimeoutMs)
		{
			msg = MSG_TIMEOUT;
			break;
		}

		// we now have less time in which to complete our request
		msg = ReadPartialRequest(uTimeoutMs - uElapsed);

		if (msg == MSG_OK)
		{
			m_stage = RDY4_PARSE;
			break;	// woohoo!
		}
		else if (msg != MSG_TIMEOUT) break;	// noooo!
		// else keep trying
	}

	return msg;
}

bool mpo_httpd::ParseRequest()
{
	bool bRes = false;

	// only proceed if this is the right place
	if (m_stage == RDY4_PARSE)
	{
		bool bGotRequestLine = false;

		try
		{
			// separate out each line
			for (;;)
			{
				string strLine;

				// Get the next line, and break if we're done
				if (!GetNextLine(strLine, m_strRequest))
				{
					// if we didn't get the request line, we're doomed
					if (!bGotRequestLine) throw runtime_error("Did not get a request line");

					// else we may be good :)
					break;
				}

				// if this is the first line (the "request line")
				if (!bGotRequestLine)
				{
					// parse the request line
					if (!GetNextWord(m_strMethod, strLine)) throw runtime_error("Error reading HTTP method");
					if (!GetNextWord(m_strURI, strLine)) throw runtime_error("Error reading URI");
					if (!GetNextWord(m_strHTTPVersion, strLine)) throw runtime_error("Error reading HTTP version");
					bGotRequestLine = true;

					// parse for convenience for the caller
					MpoHttpdUtil::ParseURI(m_strURIPath, m_strURIQuery, m_strURIFragment, m_strURI);

					// keep the URI in its escaped form so that it can be parsed in the future
				}
				else
				{
					size_t stPos = strLine.find(":");	// find separator
					if (stPos == string::npos) throw runtime_error("Separator \":\" not found");	// always should find separator
					string strName = strLine.substr(0, stPos);
					string strValue = strLine.substr(stPos+1);	// get rid of :
					if (strValue[0] == ' ') strValue = strValue.substr(1);	// get rid of whitespace which may be required but I'm not sure so I'm playing it safe
					m_mapNameValuePairs[strName] = strValue;	// add it to our map
				}
			}

			m_stage = RDY4_SET_HEADER;
			bRes = true;

			if (m_strMethod == "POST")
			{
				string strVal;

				// try to find content length
				if (GetHeader(strVal, "Content-Length"))
				{
					m_u64PostBytes = numstr::ToUint64(strVal.c_str());
					m_u64PostBytesRead = 0;
					m_stage = RDY4_RECV_POST;
				}
				// else no content length supplied, which we do not support at this time
				else
				{
					bRes = false;
					m_strLastErrorStr = "No content length was supplied with POST, which is not supported";
					m_stage = RDY4_DISCONNECT;
				}
			}

		}
		catch (std::exception &ex)
		{
			m_strLastErrorStr = ex.what();
		}
		catch (...)
		{
			m_strLastErrorStr = "Unknown exception (this should never happen)";
		}
	}
	// else this is the wrong stage
	else
	{
		m_strLastErrorStr = "ParseRequest came at an inappropriate time";
	}

	return bRes;
}

string mpo_httpd::GetMethod() const
{
	return m_strMethod;
}

string mpo_httpd::GetURI() const
{
	return m_strURI;
}

string mpo_httpd::GetPath() const
{
	return m_strURIPath;
}

string mpo_httpd::GetQuery() const
{
	return m_strURIQuery;
}

string mpo_httpd::GetFragment() const
{
	return m_strURIFragment;
}

string mpo_httpd::GetHTTPVersion() const
{
	return m_strHTTPVersion;
}

bool mpo_httpd::GetHeader(string &strValue, const string &strName)
{
	bool bRes = false;

	MapNameValue::iterator mi = m_mapNameValuePairs.find(strName);

	// if we found a match
	if (mi != m_mapNameValuePairs.end())
	{
		bRes = true;
		strValue = mi->second;
	}

	return bRes;
}

StreamMsg mpo_httpd::Recv(void *buffer, size_t stBytesCanRead, size_t *pstBytesRead, unsigned int uTimeoutMs)
{
	StreamMsg res = MSG_ERROR;

	if (pstBytesRead)
	{
		*pstBytesRead = 0;
	}

	// if we have no POST data, then return END to indicate such
	// (we can't really return ERROR because this function could be repeatedly called until we run out of POST data)
	if (m_stage != RDY4_RECV_POST) return MSG_END;

	// if we'e already read the whole post body
	if (m_u64PostBytesRead >= m_u64PostBytes)
	{
		return MSG_END;
	}

	MPO_UINT64 u64BytesLeft = m_u64PostBytes - m_u64PostBytesRead;

	size_t stBytesToRead = stBytesCanRead, stBytesRead = 0;
	if (u64BytesLeft < stBytesToRead)
	{
		stBytesToRead = (size_t) u64BytesLeft;
	}

	// If our buffer is empty, then try to fill it
	if (m_strCurBuf.empty())
	{
		res = ReadIntoBuf(uTimeoutMs);
	}
	// Else don't try to fill the buf because the POST data could be massively huge, so read it in a chunk at a time.

	// return bytes in our buf
	size_t stBufBytes = m_strCurBuf.size();
	if (stBufBytes != 0)
	{
		// if we have less bytes in our buffer than we can read, then only read what's in the buffer
		if (stBufBytes < stBytesToRead)
		{
			stBytesToRead = stBufBytes;
		}

		// fill buf
		memcpy(buffer, m_strCurBuf.data(), stBytesToRead);

		stBytesRead = stBytesToRead;
		res = MSG_OK;

		m_u64PostBytesRead += stBytesRead;
		
		// remove bytes that we've already read
		m_strCurBuf.erase(0, stBytesRead);
		
		// if we've finished reading the content length
		if (m_u64PostBytesRead == m_u64PostBytes)
		{
			m_stage = RDY4_SET_HEADER;
		}
	}
	// else if we got an error trying to fill the buf
	else if (res != MSG_TIMEOUT)
	{
		// we can't recover from this, so disconnect
		m_stage = RDY4_DISCONNECT;
	}

	if (pstBytesRead)
	{
		*pstBytesRead = stBytesRead;
	}

	return res;
}

bool mpo_httpd::SetHeaders(const mpo_httpd_headers *pHeaders)
{
	char CRLF[2] = { 0xd, 0xa };
	string strCRLF = string(CRLF, 2);
	m_pHeaders = pHeaders;

	if (m_stage != RDY4_SET_HEADER)
	{
		m_strLastErrorStr = "SetHeaders called at inappropriate time";
		return false;
	}

	// construct the outgoing headers
	m_strCurOutBuf = "HTTP/1.1 ";
	m_strCurOutBuf += numstr::ToStr(pHeaders->m_uStatusCode) + " " + pHeaders->m_strReasonPhrase + strCRLF;
	if (pHeaders->u64ContentLength != 0)
	{
		m_strCurOutBuf += "Content-Length: ";
		m_strCurOutBuf += numstr::ToStr(pHeaders->u64ContentLength) + strCRLF;
	}
	// else define chunked encoding?  TODO

	// if a content type is defined
	if (!pHeaders->m_strContentType.empty())
	{
		m_strCurOutBuf += "Content-Type: ";
		m_strCurOutBuf += pHeaders->m_strContentType + strCRLF;
	}

	// add any extra headers
	for (MapNameValue::const_iterator mi = pHeaders->m_mapOtherHeaders.begin();
		mi != pHeaders->m_mapOtherHeaders.end();
		mi++)
	{
		m_strCurOutBuf += mi->first + ": ";
		m_strCurOutBuf += mi->second + strCRLF;
	}

	// add empty line terminator
	m_strCurOutBuf += strCRLF;

	m_stage = RDY4_SEND_HEADER;
	return true;
}

StreamMsg mpo_httpd::SendHeaders(unsigned int uTimeoutMs)
{
	StreamMsg res = MSG_ERROR;

	if ((m_pHeaders != NULL) && (m_stage == RDY4_SEND_HEADER))
	{
		res = StreamFull::Write(m_pStream, m_strCurOutBuf.c_str(), m_strCurOutBuf.size(), uTimeoutMs);

		if (res == MSG_OK)
		{
			m_stage = RDY4_SEND_BODY;
		}
		// for now we don't support timing out, so it will just be a general fail
		else
		{
			switch (res)
			{
			case MSG_TIMEOUT:
				m_strLastErrorStr = "SendHeaders didn't finish promptly";
				break;
			case MSG_END:
				m_strLastErrorStr = "Client disconnected";
				break;
			default:
				m_strLastErrorStr = "Error writing to stream: ";
				m_strLastErrorStr += net_GetLastErrorStr();
				break;
			}

			res = MSG_ERROR;
		}
	}
	else
	{
		if (m_pHeaders == NULL)
		{
			m_strLastErrorStr = "SetHeaders apparently was not called";
		}
		else
		{
			m_strLastErrorStr = "SendHeaders called at an inappropriate time";
		}
	}

	return res;
}

StreamMsg mpo_httpd::Send(const void *buffer, size_t bytes_can_send, size_t *bytes_sent, unsigned int uTimeoutMs)
{
	unsigned int uStartTime = MpoTimerUtil::RefreshTimer();
	StreamMsg msg = MSG_ERROR;

	// if the entire content has been sent
	if ((m_stage == RDY4_DISCONNECT) || (m_stage == RDY4_REQUEST))
	{
		return MSG_END;
	}
	// if they call this at the wrong time, then error out immediately
	else if (m_stage != RDY4_SEND_BODY)
	{
		m_strLastErrorStr = "Send came at inappropriate time";
		return MSG_ERROR;
	}

	*bytes_sent = 0;
	MPO_UINT64 u64BytesCanSend = bytes_can_send;	// so that we aren't getting warnings about "possible loss of data"

	// check to make sure caller isn't going to exceed
	MPO_UINT64 u64TotalProposedBytes = u64BytesCanSend + m_u64BodyBytesSent;
	if (u64TotalProposedBytes > m_pHeaders->u64ContentLength)
	{
		MPO_UINT64 u64ExcessBytes = u64TotalProposedBytes - m_pHeaders->u64ContentLength;
		u64BytesCanSend -= u64ExcessBytes;

		// if they can't send anything
		if (u64BytesCanSend == 0)
		{
			return MSG_END;
		}
	}

	unsigned int uElapsed = 0;

	// go until we timeout or fail
	for (;;)
	{
		// we now have less time in which to complete our request
		size_t stBytesSent = m_pStream->Write((unsigned char *) buffer + *bytes_sent, (size_t) u64BytesCanSend - *bytes_sent, (uTimeoutMs - uElapsed));
		msg = m_pStream->GetLastMsg();

		if (msg == MSG_OK)
		{
			*bytes_sent += stBytesSent;
			// if we've emptied our buffer
			if (*bytes_sent == u64BytesCanSend)
			{
				break;	// woohoo!
			}
		}
		else if (msg == MSG_ERROR)
		{
			m_strLastErrorStr = net_GetLastErrorStr();
			break;
		}
		else if (msg == MSG_END)
		{
			m_strLastErrorStr = "Client disconnected";
			break;
		}
		// else it's a timeout so keep trying

		// IMPORTANT:
		// This recalculation of uElapsed comes after the call to m_pStream->Write to ensure that m_pStream->Write is called at least one time.
		// Otherwise, if the CPU is saturated, m_pStream->Write may never be called.
		uElapsed = MpoTimerUtil::GetElapsedMs(uStartTime);
		if (uElapsed > uTimeoutMs)
		{
			msg = MSG_TIMEOUT;
			break;
		}
	}

	m_u64BodyBytesSent += *bytes_sent;

	if (m_u64BodyBytesSent >= m_pHeaders->u64ContentLength)
	{
		string strValue;

		m_stage = RDY4_REQUEST;	// reset for next request

		// if client requested the connection be closed
		if (GetHeader(strValue, "Connection"))
		{
			if (mpom::str_case_eq(strValue,"close"))
			{
				m_stage = RDY4_DISCONNECT;
			}
		}

		msg = MSG_END;

		m_u64BodyBytesSent = 0;	// reset for the next time around
	}

	return msg;
}

string mpo_httpd::GetLastErrorStr()
{
	return m_strLastErrorStr;
}

string MpoHttpdUtil::Unescape(const string &strEscaped)
{
	string strRes;

	string::const_iterator si = strEscaped.begin();

	while (si != strEscaped.end())
	{
		char ch = *si;

		// if this character isn't escaped
		if (ch != '%')
		{
			strRes += ch;
		}
		// else if it is escaped
		else
		{
			string strHex;

			si++;	// skip %
			if (si == strEscaped.end())	break;
			strHex += *si;
			si++;
			if (si == strEscaped.end())	break;
			strHex += *si;
			unsigned int u = numstr::ToUint32(strHex.c_str(), 16);
			strRes += (char) u;
		}

		si++;
	}

	return strRes;
}

void MpoHttpdUtil::ParseURI(string &strDstPath, string &strDstQuery, string &strDstFragment, const string &strSrcURI)
{
	bool bParseOK = true;

	strDstPath.clear();
	strDstQuery.clear();
	strDstFragment.clear();
	size_t stIdxQuery = strSrcURI.find("?");
	size_t stIdxFragment = strSrcURI.find("#");

	// if a query exists
	if (stIdxQuery != string::npos)
	{
		strDstPath = Unescape(strSrcURI.substr(0, stIdxQuery));
		
		// if a fragment exists
		if (stIdxFragment != string::npos)
		{
			// sanity check, make sure query comes before the fragment
			if (stIdxFragment > stIdxQuery)
			{
				strDstQuery = Unescape(strSrcURI.substr(stIdxQuery + 1, stIdxFragment - stIdxQuery - 1));
				strDstFragment = Unescape(strSrcURI.substr(stIdxFragment + 1));
			}
			else
			{
				bParseOK = false;
			}
		}
		// else no fragment, just a query
		else
		{
			strDstQuery = Unescape(strSrcURI.substr(stIdxQuery + 1));
		}
	}
	// else if a fragment exists without a query
	else if (stIdxFragment != string::npos)
	{
		strDstPath = Unescape(strSrcURI.substr(0, stIdxFragment));
		strDstFragment = Unescape(strSrcURI.substr(stIdxFragment + 1));
	}
	// else no query or fragment
	else
	{
		strDstPath = Unescape(strSrcURI);
	}

	if (!bParseOK)
	{
		throw runtime_error("parse failed");
	}
}

///

mpo_httpd::mpo_httpd() :
m_stage(RDY4_REQUEST),
m_pHeaders(NULL),
m_u64BodyBytesSent(0),
m_u64PostBytesRead(0)
{
}

mpo_httpd::~mpo_httpd()
{
}

bool mpo_httpd::Init(INonblockingStream *pStream)
{
	m_pStream = pStream;
	return true;
}

StreamMsg mpo_httpd::ReadIntoBuf(unsigned int uTimeoutMs)
{
	StreamMsg res = MSG_ERROR;
	unsigned char buf[320];

	size_t stBytesRead = m_pStream->Read(buf, sizeof(buf), uTimeoutMs);
	res = m_pStream->GetLastMsg();

	if (res == MSG_OK)
	{
		m_strCurBuf += string((char *) buf, stBytesRead);
	}
	// else if we got an error or disconnect
	else if (res == MSG_ERROR)
	{
		m_strLastErrorStr = "Error when reading: ";
		m_strLastErrorStr += net_GetLastErrorStr();
	}
	// else timeout or disconnect

	return res;
}

// miniature version of isspace() because we don't want to consider linefeeds/carriage returns to be whitespace
bool my_is_whitespace(char ch)
{
	if ((ch == ' ') || (ch == '\t')) return true;
	else return false;
}

bool mpo_httpd::GetNextLine(string &strDst, string &strSrc)
{
	bool bRes = false;
	char ch = 0;
	unsigned int idx = 0;

	// go until we reach end of the buffer (or end of line)
	while (idx < strSrc.size())
	{
		ch = strSrc[idx];

		// if we have hit the end of the line
		if ((ch == 10) || (ch == 13))
		{
			// keep reading until we get passed the end of line chars
			while (((ch == 10) || (ch == 13)) && (idx < strSrc.size()))
			{
				idx++;
				ch = strSrc[idx];
			}
			
			break;	// we got our line
		}

		// if the character is part of the current line then add it
		else
		{
			strDst += ch;
		}
		idx++;
	}

	bRes = !strDst.empty();
	strSrc = strSrc.substr(idx);
	
	return bRes;
}

bool mpo_httpd::GetNextWord(string &strDst, string &strSrc)
{
	bool result = false;
	unsigned int index = 0;
	int start_index = 0;

	// find beginning of word, skip spaces or tabs
	while ((my_is_whitespace(strSrc[index])) && (index < strSrc.size()))
	{
		index++;
	}

	// make sure we aren't at the end of the string already
	if (index < strSrc.size())
	{
		start_index = index;

		// now go to the end of the current word
		while ((!my_is_whitespace(strSrc[index])) && (index < strSrc.size()))
		{
			index++;
		}

		strDst = strSrc.substr(start_index, index-start_index);
		strSrc = strSrc.substr(index);
		result = true;
	}

	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////

IMpoHttpdListenerSPtr MpoHttpdListenerFactory::CreateInstance(unsigned int uListenPort, void *(*pCallback)(void*), void *data)
{
	return mpo_httpd_listener::CreateInstance(uListenPort, pCallback, data);
}

IMpoHttpdListenerSPtr mpo_httpd_listener::CreateInstance(unsigned int uListenPort, void *(*pCallback)(void*), void *data)
{
	IMpoHttpdListenerSPtr pRes;
	string errorMsg;

	mpo_httpd_listener *pInstance = new mpo_httpd_listener();

	pInstance->m_srv = MpoServerFactory::CreateInstance();
	pInstance->m_pServer = pInstance->m_srv.get();

	// try to bind to the port
	try
	{
		pInstance->m_pServer->Initialize(uListenPort);
		pInstance->m_pCallback = pCallback;
		pInstance->data = data;
		pInstance->m_comm.pInstance = pInstance;    // so child thread can use private instance variables

		// start listener thread
		if (mpo_create_thread(&pInstance->m_listenerID, ListenerThread, &pInstance->m_comm)) {
			pRes = MpoHttpdListenerSPtr(pInstance, mpo_httpd_listener::deleter());
		}
			// else thread creation failed
		else {
			errorMsg = "Thread creation failed";
		}
	}
		// else binding failed
	catch (std::exception &) {
		errorMsg = "Could not listen on port ";
		errorMsg += numstr::ToStr(uListenPort);
	}

	// if we failed, then clean up properly
	if (pRes.get() == NULL)
	{
		if (pInstance)
		{
			delete pInstance;
			throw runtime_error(errorMsg.c_str());
		}
	}

	return pRes;
}

mpo_httpd_listener::mpo_httpd_listener() :
	m_pCallback(NULL),
	m_listenerID(0)
{
}

mpo_httpd_listener::~mpo_httpd_listener()
{
	// if child thread is/was running, then make sure it shuts down
	if (m_listenerID != 0)
	{
		// shut down listener thread
		m_comm.m_bParentRequestedQuit = true;

		// Wait for child to tell us that it has quit.
		// TODO : after a maximum period of time, forcefully kill the child thread
		while (!m_comm.m_bChildHasQuit)
		{
			MpoTimerUtil::MakeDelay(1);
		}
		
		// free up resources
		mpo_wait_thread(&m_listenerID);
	}
}

void *mpo_httpd_listener::ListenerThread(void *pListenerComm)
{
	CListenerComm *pComm = (CListenerComm *) pListenerComm;

	// to keep track of connections so we can close them when the child thread has exited
	map <mpo_threadID, CHttpdListenerStorage> mapThreads;

	// go until parent requests that we quit
	while (!pComm->m_bParentRequestedQuit)
	{
		// see if we have a new connection
		mpo_sockpres_autoptr socket = pComm->pInstance->m_pServer->Accept(125);

		// if we have a connection, then spin off a thread to deal with it
		if (socket.get() != NULL)
		{
			CHttpdListenerStorage storage;
			storage.m_threadCommSPtr = CHttpdThreadComm::CreateInstance();
			storage.m_pThreadComm = storage.m_threadCommSPtr.get();
			CHttpdThreadComm *pCommChild = storage.m_pThreadComm;
			storage.m_stream = MpoNetStreamFactory::CreateInstance(socket);
			if (storage.m_stream.get())
			{
				storage.m_httpdSPtr = mpo_httpd::CreateInstance(storage.m_stream.get());
				pCommChild->pHTTPD = storage.m_httpdSPtr.get();
				pCommChild->m_pCallback = pComm->pInstance->m_pCallback;
				pCommChild->data = pComm->pInstance->data;
				if (pCommChild->pHTTPD != NULL)
				{
					mpo_threadID id;
					if (mpo_create_thread(&id, ListenerThreadHelper, pCommChild))
					{
						// success, add to our map
						// TODO : log

						mapThreads[id] = storage;
					}
					// else create thread failed (this should never happen)
					else
					{
						// TODO : log
					}
				}
				// httpd instance failed (this should never happen)
				else
				{
					// TODO: log error
				}
			}
			// stream instance failed (this should never happen)
			else
			{
				// TODO: log error
			}
		}
		// else no incoming connection, so wait for next connection or parent thread to signal us to exit

		// check to see if any of our child threads have quit since last time
		for (map <mpo_threadID, CHttpdListenerStorage>::iterator mi = mapThreads.begin();
			mi != mapThreads.end(); mi++)
		{
			if (mi->second.m_pThreadComm->m_bChildHasQuit)
			{
				// do a formal wait for the child thread to exit.  This is necessary to free up the resources used by the thread (eg 8MB on linux).
#ifndef NDEBUG
				bool bSuccess =
#endif
				mpo_wait_thread(&mi->first);
				assert(bSuccess);

				// make sure we don't call mpo_wait_thread again later
				mapThreads.erase(mi);

				// to avoid an unknown and potentially unstable state with our iterator, we
				// will only erase one thread per check.
				break;
			}
		}

	} // end if parent hasn't asked us to quit

	// now signal all remaining children threads to exit (it's ok if child has already exited)
	for (map <mpo_threadID, CHttpdListenerStorage>::iterator mi = mapThreads.begin();
		mi != mapThreads.end(); mi++)
	{
		mi->second.m_pThreadComm->m_bParentRequestedQuit = true;
	}

	// now wait for all child thread to indicate that they've exited (they all may have already)
	for (map <mpo_threadID, CHttpdListenerStorage>::iterator mi = mapThreads.begin();
		mi != mapThreads.end(); mi++)
	{
		// TODO : wait a maximum amount of time, then kill the thread forcefully
		while (!mi->second.m_pThreadComm->m_bChildHasQuit)
		{
			MpoTimerUtil::MakeDelay(1);
		}

		// clean up resources used by the thread
#ifndef NDEBUG
		bool bSuccess =
#endif
		mpo_wait_thread(&mi->first);
		assert(bSuccess);
	}

	// tell our parent that we're done
	pComm->m_bChildHasQuit = true;

	return NULL;
}

// helper intermediate callback to ensure that m_bChildHasQuit will always be set when the child thread exits
void *mpo_httpd_listener::ListenerThreadHelper(void *pHttpThreadComm)
{
	CHttpdThreadComm *pComm = (CHttpdThreadComm *) pHttpThreadComm;

	// run the child callback
	void *pRetVal = pComm->m_pCallback(pHttpThreadComm);

	// ensure that this bool is set when the child returns (so that the child doesn't have to worry about setting it, which could lead to errors)
	pComm->m_bChildHasQuit = true;

	return pRetVal;
}
