#include "test_headers.h"

class CCliData
{
public:
	CCliData(unsigned int uPort) :
	  m_uPort(uPort),
	  m_bRequestDisconnect(true),
		m_bClientError(false),
		m_bClientDone(false)
	{
	}

	unsigned int m_uPort;
	bool m_bRequestDisconnect;

	bool m_bClientError;
	bool m_bClientDone;
};

void *http_client_thread(void *pData)
{
	CCliData *pDat = (CCliData *) pData;
	
	try
	{
		IMpoClientSPtr cli = MpoClientFactory::CreateInstance();
		IMpoClient *pClient = cli.get();

		net_result res = pClient->connect_and_wait("127.0.0.1", pDat->m_uPort);
		if (res != NET_OK)
		{
			throw false;
		}

		nonblocking_sharedptr stream = pClient->get_stream();
		IMpoHttpSPtr hSPtr = MpoHttpFactory::CreateInstance(stream.get(), "TestClient");
		IMpoHttp *pHTTP = hSPtr.get();

		list<string> lstPathSegments;
		lstPathSegments.push_back("a b c");
		string strURI = MpoHttpUtil::MakeURI(&lstPathSegments);
		res = pHTTP->StartGet("myhost", strURI, pDat->m_bRequestDisconnect, 5000);
		if (res != NET_OK)
		{
			throw false;
		}

		// parse the headers
		StreamMsg msg = pHTTP->GetHeaders(5000);
		if (msg != MSG_OK) throw false;

		unsigned int u = pHTTP->GetResultCode();
		if (u != 200) throw false;

		string strBuf;
		msg = pHTTP->FillBuf(strBuf, 5000);

		if (strBuf != "HI THERE") throw false;

		// if server isn't supposed to disconnect us, then request something else to ensure server is still with us
		if (!pDat->m_bRequestDisconnect)
		{
			res = pHTTP->StartGet("myhost2", strURI, true, 5000);	// always disconnect the second time around
			if (res != NET_OK)
			{
				throw false;
			}

			msg = pHTTP->GetHeaders(5000);
			if  (msg != MSG_OK)
			{
				throw false;
			}

			unsigned int u = pHTTP->GetResultCode();
			if (u != 200)
			{
				throw false;
			}

			msg = pHTTP->FillBuf(strBuf, 5000);

			if (strBuf != "2HI THERE")
			{
				throw false;
			}
		}

		// client destructor will close socket
	}
	catch (...)
	{
		pDat->m_bClientError = true;
	}

	pDat->m_bClientDone = true;

	return NULL;
}

void test_httpd1()
{
	IMpoServerSPtr srv = MpoServerFactory::CreateInstance();
	IMpoServer *pServer = srv.get();

	unsigned int uPort = 5555;
	pServer->Initialize(uPort);

	CCliData dat(uPort);

	mpo_threadID id;
	bool bRes = mpo_create_thread(&id, http_client_thread, &dat);
	
	TEST_REQUIRE(bRes);

	// wait for client connection
	mpo_sockpres_autoptr sock = pServer->Accept(5000);

	// make sure we got a connection
	TEST_CHECK(sock.get() != NULL);

	nonblocking_sharedptr stream = MpoNetStreamFactory::CreateInstance(sock);
	IMpoHttpdSPtr hSPtr = MpoHttpdFactory::CreateInstance(stream.get());
	IMpoHttpd *pHTTPD = hSPtr.get();

	TEST_CHECK(pHTTPD != NULL);

	// try calling ParseRequest out of order; it should fail
	bRes = pHTTPD->ParseRequest();
	TEST_CHECK(!bRes);

	StreamMsg msg = pHTTPD->ReadRequest(5000);
	TEST_CHECK_EQUAL(msg, MSG_OK);

	// call ReadRequest out of order
	msg = pHTTPD->ReadRequest(15000);
	TEST_CHECK_EQUAL(msg, MSG_ERROR);

	string s = pHTTPD->GetMethod();
	TEST_CHECK_EQUAL("", s);

	bRes = pHTTPD->ParseRequest();
	TEST_CHECK(bRes);

	s = pHTTPD->GetMethod();
	TEST_CHECK_EQUAL("GET", s);

	s = pHTTPD->GetURI();
	TEST_CHECK_EQUAL("/a%20b%20c", s);

	s = pHTTPD->GetHTTPVersion();
	TEST_CHECK_EQUAL("HTTP/1.1", s);

	bRes = pHTTPD->GetHeader(s, "Host");
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(s, "myhost");

	// try to send headers without first setting them
	msg = pHTTPD->SendHeaders(5000);
	TEST_CHECK_EQUAL(MSG_ERROR, msg);

	string strPayload = "HI THERE!";
	mpo_httpd_headers hdr(200, "OK");
	hdr.u64ContentLength = strPayload.size() - 1;	// don't send the final '!', this tests to make sure our Send function is watching the content length
	hdr.m_strContentType = "text/plain; charset=UTF8";
	bRes = pHTTPD->SetHeaders(&hdr);
	TEST_CHECK(bRes);

	msg = pHTTPD->SendHeaders(5000);
	TEST_CHECK_EQUAL(MSG_OK, msg);

	size_t stBytesSent = 0;
	msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
	TEST_CHECK_EQUAL(MSG_END, msg);
	TEST_CHECK_EQUAL(stBytesSent, strPayload.size() - 1);

	// try to send more bytes even though we have exceeded our content size
	stBytesSent = 0;
	msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
	TEST_CHECK_EQUAL(MSG_END, msg);
	TEST_CHECK_EQUAL(stBytesSent, 0);

	// try reading a request when the client requested disconnect
	msg = pHTTPD->ReadRequest(5000);
	TEST_CHECK_EQUAL(msg, MSG_END);

	// Wait for client to be done.
	// TODO : change this to something more efficient
	while (!dat.m_bClientDone)
	{
		make_delay(10);
	}

	// clean-up
	// Socket will automatically be closed by socket presenter class.

	// make sure client thread didn't get any errors
	TEST_CHECK(dat.m_bClientDone);
	TEST_CHECK(!dat.m_bClientError);
}

TEST_CASE(httpd1)
{
	test_httpd1();
}


void test_httpd2()
{
	IMpoServerSPtr srv = MpoServerFactory::CreateInstance();
	IMpoServer *pServer = srv.get();

	string s;

	unsigned int uPort = 5555;
	pServer->Initialize(uPort);

	CCliData dat(uPort);
	dat.m_bRequestDisconnect = false;	// don't disconnect

	mpo_threadID id;
	bool bRes = mpo_create_thread(&id, http_client_thread, &dat);
	
	TEST_REQUIRE(bRes);

	// wait for client connection
	mpo_sockpres_autoptr sock = pServer->Accept(5000);

	// make sure we got a connection
	TEST_CHECK(sock.get() != NULL);

	nonblocking_sharedptr stream = MpoNetStreamFactory::CreateInstance(sock);
	IMpoHttpdSPtr hSPtr = MpoHttpdFactory::CreateInstance(stream.get());
	IMpoHttpd *pHTTPD = hSPtr.get();

	TEST_CHECK(pHTTPD != NULL);

	StreamMsg msg = pHTTPD->ReadRequest(5000);
	TEST_CHECK_EQUAL(msg, MSG_OK);

	bRes = pHTTPD->ParseRequest();
	TEST_CHECK(bRes);

	s = pHTTPD->GetMethod();
	TEST_CHECK_EQUAL("GET", s);

	s = pHTTPD->GetURI();
	TEST_CHECK_EQUAL("/a%20b%20c", s);

	s = pHTTPD->GetHTTPVersion();
	TEST_CHECK_EQUAL("HTTP/1.1", s);

	bRes = pHTTPD->GetHeader(s, "Host");
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(s, "myhost");

	string strPayload = "HI THERE";
	mpo_httpd_headers hdr(200, "OK");
	hdr.u64ContentLength = strPayload.size();
	hdr.m_strContentType = "text/plain; charset=UTF8";
	bRes = pHTTPD->SetHeaders(&hdr);
	TEST_CHECK(bRes);

	msg = pHTTPD->SendHeaders(5000);
	TEST_CHECK_EQUAL(MSG_OK, msg);

	size_t stBytesSent = 0;
	msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
	TEST_CHECK_EQUAL(MSG_END, msg);
	TEST_CHECK_EQUAL(stBytesSent, strPayload.size());

	// try to send more bytes even though we have exceeded our content size
	stBytesSent = 0;
	msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
	TEST_CHECK_EQUAL(MSG_END, msg);
	TEST_CHECK_EQUAL(stBytesSent, 0);

	// try reading a request since the client has another one coming
	msg = pHTTPD->ReadRequest(5000);
	TEST_REQUIRE_EQUAL(msg, MSG_OK);
	bRes = pHTTPD->ParseRequest();
	TEST_CHECK(bRes);
	hdr.m_uStatusCode = 200;
	strPayload = "2HI THERE";
	hdr.u64ContentLength = strPayload.size();
	bRes = pHTTPD->SetHeaders(&hdr);
	TEST_CHECK(bRes);
	msg = pHTTPD->SendHeaders(5000);
	if (msg == MSG_ERROR)
	{
		s = net_GetLastErrorStr();
	}
	TEST_CHECK_EQUAL(MSG_OK, msg);
	stBytesSent = 0;
	msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
	TEST_CHECK_EQUAL(MSG_END, msg);
	TEST_CHECK_EQUAL(stBytesSent, strPayload.size());

	// Wait for client to be done.
	// TODO : change this to something more efficient
	while (!dat.m_bClientDone)
	{
		make_delay(10);
	}

	// clean-up
	// Socket will automatically be closed by socket presenter class.

	// make sure client thread didn't get any errors
	TEST_CHECK(dat.m_bClientDone);
	TEST_CHECK(!dat.m_bClientError);
}

TEST_CASE(httpd2)
{
	test_httpd2();
}


/////////////

void *http_post_thread(void *dontcare)
{
	IMpoClientSPtr cli = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = cli.get();

	try
	{
		net_result res = pClient->connect_and_wait("127.0.0.1", 5555);
		if (res != NET_OK) throw "connect failed";

		// send post body
		nonblocking_sharedptr StreamSPtr = pClient->get_stream();
		INonblockingStream *pStream = StreamSPtr.get();

		char szReq[] =
			"POST /path/script.cgi HTTP/1.0\r\n"
			"Content-Type: application/octet-stream\r\n"
			"Content-Length: 4\r\n"
			"\r\n"
			"ABCD";

		StreamFull::Write(pStream, szReq, sizeof(szReq), 5000);
	}
	catch (...)
	{
	}

	return NULL;
}

void test_httpd_post()
{
	IMpoServerSPtr srv = MpoServerFactory::CreateInstance();
	IMpoServer *pServer = srv.get();

	string s;

	unsigned int uPort = 5555;
	pServer->Initialize(uPort);

	CCliData dat(uPort);
	dat.m_bRequestDisconnect = false;	// don't disconnect

	mpo_threadID id;
	bool bRes = mpo_create_thread(&id, http_post_thread, NULL);
	
	TEST_REQUIRE(bRes);

	// wait for client connection
	mpo_sockpres_autoptr sock = pServer->Accept(5000);

	// make sure we got a connection
	TEST_CHECK(sock.get() != NULL);

	nonblocking_sharedptr stream = MpoNetStreamFactory::CreateInstance(sock);
	IMpoHttpdSPtr hSPtr = MpoHttpdFactory::CreateInstance(stream.get());
	IMpoHttpd *pHTTPD = hSPtr.get();

	TEST_CHECK(pHTTPD != NULL);

	StreamMsg msg = pHTTPD->ReadRequest(5000);
	TEST_CHECK_EQUAL(msg, MSG_OK);

	bRes = pHTTPD->ParseRequest();
	TEST_CHECK(bRes);

	s = pHTTPD->GetMethod();
	TEST_CHECK_EQUAL("POST", s);

	s = pHTTPD->GetURI();
	TEST_CHECK_EQUAL("/path/script.cgi", s);

	s = pHTTPD->GetHTTPVersion();
	TEST_CHECK_EQUAL("HTTP/1.0", s);

	// try to set the header before reading the POST body (should be an error)
	mpo_httpd_headers hdr(200, "OK");
	bRes = pHTTPD->SetHeaders(&hdr);
	TEST_CHECK(!bRes);

	char buf[80];
	size_t stBytesRead = 0;
	msg = pHTTPD->Recv(buf, 2, &stBytesRead, 5000);
	TEST_CHECK_EQUAL(MSG_OK, msg);
	TEST_CHECK_EQUAL(2, stBytesRead);
	msg = pHTTPD->Recv(buf + 2, 2, &stBytesRead, 5000);
	TEST_CHECK_EQUAL(MSG_OK, msg);
	TEST_CHECK_EQUAL(2, stBytesRead);
	msg = pHTTPD->Recv(buf + 4, 1, &stBytesRead, 5000);
	TEST_CHECK_EQUAL(MSG_END, msg);
	TEST_CHECK_EQUAL(0, stBytesRead);

	s = string(buf, 4);
	TEST_CHECK_EQUAL("ABCD", s);

	// clean-up
	// Socket will automatically be closed by socket presenter class.
}

TEST_CASE(httpd_post)
{
	test_httpd_post();
}

/////////////

// global so that two threads can use it, for convenience
bool g_bListenerTestSuccess = false;

void *listener_callback(void *pData)
{
	IHttpdThreadComm *pComm = (IHttpdThreadComm *) pData;

	IMpoHttpd *pHTTPD = pComm->GetHttpdInterface();

	try
	{
		if ((size_t) pComm->GetUserData() != 1234) throw "http thread comm's data was unexpected";

		StreamMsg msg = pHTTPD->ReadRequest(5000);
		if (msg != MSG_OK) throw "ReadRequest did not return OK";

		bool bRes = pHTTPD->ParseRequest();
		if (!bRes) throw "ParseRequest failed";

		string s = pHTTPD->GetMethod();
		if (s != "GET") throw "Unknown method";

		s = pHTTPD->GetURI();
		if (s != "/") throw "Unexpected URI";

		string strPayload = "LISTENER";
		mpo_httpd_headers hdr(200, "OK");
		hdr.u64ContentLength = strPayload.size();
		hdr.m_strContentType = "text/plain; charset=UTF8";
		bRes = pHTTPD->SetHeaders(&hdr);
		if (!bRes) throw "SetHeaders failed";

		msg = pHTTPD->SendHeaders(5000);
		if (MSG_OK != msg) throw "SendHeaders failed";

		size_t stBytesSent = 0;
		msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
		if (MSG_END != msg) throw "Send did not return MSG_END";
		if (stBytesSent != strPayload.size()) throw "stBytesSend != payload size";

		g_bListenerTestSuccess = true;
	}
//	catch (const char *pszErr)
	catch (...)
	{
		// modify this if necessary to read the error
		g_bListenerTestSuccess = false;
	}

	return NULL;
}

void test_httpd_listener()
{
	IMpoHttpdListenerSPtr ListenerSPtr = MpoHttpdListenerFactory::CreateInstance(5555, listener_callback, (void *) 1234);
	string s;

	IMpoClientSPtr cli = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = cli.get();

	// connect to the listener which will now be running on a background thread
	net_result res = pClient->connect_and_wait("127.0.0.1", 5555);
	TEST_REQUIRE(res == NET_OK);

	nonblocking_sharedptr stream = pClient->get_stream();
	INonblockingStream *pStream = stream.get();
	IMpoHttpSPtr hSPtr = MpoHttpFactory::CreateInstance(pStream, "TestClient");
	IMpoHttp *pHTTP = hSPtr.get();

	res = pHTTP->StartGet("myhost3", "/", true, 5000);
	TEST_REQUIRE (res == NET_OK);

	// parse the headers
	StreamMsg msg = pHTTP->GetHeaders(5000);
	TEST_REQUIRE (msg == MSG_OK);

	unsigned int u = pHTTP->GetResultCode();
	TEST_REQUIRE_EQUAL(u, 200);

	string strBuf;
	msg = pHTTP->FillBuf(strBuf, 5000);

	TEST_REQUIRE_EQUAL(strBuf, "LISTENER");

	// wait for disconnect from server
	hSPtr.reset();
	for (;;)
	{
		unsigned char buf[1024];
		pStream->Read(buf, sizeof(buf), 1000);
		StreamMsg msg = pStream->GetLastMsg();
		if (msg == MSG_END)
		{
			break;
		}
		else if (msg != MSG_TIMEOUT)
		{
			// we don't really expect this to happen but if it does, we'll log it
			TEST_CHECK_EQUAL(MSG_END, msg);
			break;
		}
	}

	// wait for thread to exit
	ListenerSPtr.reset();

	// all done, make sure we passed the test
	TEST_REQUIRE(g_bListenerTestSuccess);
}

TEST_CASE(httpd_listener)
{
	test_httpd_listener();
}

//////////////////////////////////////////////////////////////////////////////////////////////

void *listener2_callback(void *pData)
{
	IHttpdThreadComm *pComm = (IHttpdThreadComm *) pData;

	// wait for parent to signal us to quit
	while (!pComm->IsQuitRequested())
	{
		make_delay(1);
	}

	return NULL;
}

void test_httpd_listener2()
{
	IMpoHttpdListenerSPtr ListenerSPtr = MpoHttpdListenerFactory::CreateInstance(5555, listener2_callback, (void *) NULL);
	string s;

	IMpoClientSPtr cli = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = cli.get();

	// connect to the listener which will now be running on a background thread
	net_result res = pClient->connect_and_wait("127.0.0.1", 5555);
	TEST_REQUIRE(res == NET_OK);

	// wait for thread to exit
	ListenerSPtr.reset();

	// we know we passed the test if an assertion was not thrown
}

TEST_CASE(httpd_listener2)
{
	test_httpd_listener2();
}

/////////////////

string g_strReadBuf;
size_t MyRead(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs)
{
	size_t stBytesToCopy = g_strReadBuf.size();
	if (stBytesToRead < stBytesToCopy)
	{
		stBytesToCopy = stBytesToRead;
	}

	if (stBytesToCopy > 0)
	{
		memcpy(buf, g_strReadBuf.data(), stBytesToCopy);
		g_strReadBuf.erase(0, stBytesToCopy);	// erase bytes that we've sent
	}

	return stBytesToCopy;
}

string g_strWriteBuf;
size_t g_stWriteReturnCode = 0;
bool g_bOverrideReturnCode = false;
size_t MyWrite(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs)
{
	g_strWriteBuf += string((const char *) buf, stBytesToWrite);

	if (g_bOverrideReturnCode)
	{
		return g_stWriteReturnCode;
	}
	else
	{
		return stBytesToWrite;
	}
}

StreamMsg g_MsgResponse = MSG_ERROR;
StreamMsg MyGetLastMsg()
{
	return g_MsgResponse;
}

void test_httpd_send_defect()
{
	g_bOverrideReturnCode = false;
	g_MsgResponse = MSG_OK;

	nonblocking_sharedptr streamSPtr = NonblockingStreamCallbacks::GetInstance(MyRead,
		MyWrite, MyGetLastMsg);
	INonblockingStream *pStream = streamSPtr.get();
	TEST_REQUIRE(pStream);

	IMpoHttpdSPtr httpdSPtr = MpoHttpdFactory::CreateInstance(pStream);
	IMpoHttpd *pHTTPD = httpdSPtr.get();
	TEST_REQUIRE(pHTTPD);

	g_strReadBuf = "GET / HTTP/1.1\r\n"
		"Host: http://localhost\r\n"
		"\r\n";
	StreamMsg msg = pHTTPD->ReadRequest(5000);
	TEST_REQUIRE_EQUAL(MSG_OK, msg);

	bool bRes = pHTTPD->ParseRequest();
	TEST_REQUIRE(bRes);

	string s = pHTTPD->GetMethod();
	TEST_REQUIRE_EQUAL("GET", s);

	string strPayload = "LISTENER";
	mpo_httpd_headers hdr(200, "OK");
	hdr.u64ContentLength = strPayload.size();
	hdr.m_strContentType = "text/plain; charset=UTF8";
	hdr.m_mapOtherHeaders["MyHeader"] = "testing";	// test ability to add arbitrary headers
	bRes = pHTTPD->SetHeaders(&hdr);
	TEST_REQUIRE(bRes);

	msg = pHTTPD->SendHeaders(5000);
	TEST_REQUIRE_EQUAL(MSG_OK, msg);

	// examine write buffer to see if we got our custom header
	size_t stRes = g_strWriteBuf.find("MyHeader: testing");
	TEST_CHECK(stRes != string::npos);

	g_stWriteReturnCode = -1;
	g_bOverrideReturnCode = true;
	g_MsgResponse = MSG_ERROR;

	size_t stBytesSent = 0;
	msg = pHTTPD->Send(strPayload.data(), strPayload.size(), &stBytesSent, 25000);
	TEST_CHECK_EQUAL(MSG_ERROR, msg);

	// the defect to be fixed is: stBytesSent would be set to -1 at this point when it shouldn't be
	TEST_CHECK_EQUAL(0, stBytesSent);

	// clean-up
	httpdSPtr.reset();
}

TEST_CASE(httpd_send_defect)
{
	test_httpd_send_defect();
}

///////////////////////////////////////////////

void test_uri_parse1()
{
	string strDstPath, strDstQuery, strDstFragment;
	bool bRes = false;

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "/bl%20ah?q=3%205#h%20i");
	TEST_CHECK_EQUAL("/bl ah", strDstPath);
	TEST_CHECK_EQUAL("q=3 5", strDstQuery);
	TEST_CHECK_EQUAL("h i", strDstFragment);

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "/bl%20ah?q=3%205");
	TEST_CHECK_EQUAL("/bl ah", strDstPath);
	TEST_CHECK_EQUAL("q=3 5", strDstQuery);
	TEST_CHECK_EQUAL("", strDstFragment);

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "/bl%20ah#h%20i");
	TEST_CHECK_EQUAL("/bl ah", strDstPath);
	TEST_CHECK_EQUAL("", strDstQuery);
	TEST_CHECK_EQUAL("h i", strDstFragment);

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "/bl%20ah");
	TEST_CHECK_EQUAL("/bl ah", strDstPath);
	TEST_CHECK_EQUAL("", strDstQuery);
	TEST_CHECK_EQUAL("", strDstFragment);

	// misbehavior tested here
	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "?q#");
	TEST_CHECK_EQUAL("", strDstPath);
	TEST_CHECK_EQUAL("q", strDstQuery);
	TEST_CHECK_EQUAL("", strDstFragment);

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "");
	TEST_CHECK_EQUAL("", strDstPath);
	TEST_CHECK_EQUAL("", strDstQuery);
	TEST_CHECK_EQUAL("", strDstFragment);

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "a?#");
	TEST_CHECK_EQUAL("a", strDstPath);
	TEST_CHECK_EQUAL("", strDstQuery);
	TEST_CHECK_EQUAL("", strDstFragment);

	MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "?#b");
	TEST_CHECK_EQUAL("", strDstPath);
	TEST_CHECK_EQUAL("", strDstQuery);
	TEST_CHECK_EQUAL("b", strDstFragment);

	TEST_CHECK_THROW(MpoHttpdUtil::ParseURI(strDstPath, strDstQuery, strDstFragment, "#?asdfg"));
}

TEST_CASE(uri_parse1)
{
	test_uri_parse1();
}
