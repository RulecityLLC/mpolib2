#include "test_headers.h"

TEST_CASE(server1)
{
	unsigned int uPort = 10000;
	bool bRes = false;

	IMpoServerSPtr srv = MpoServerFactory::CreateInstance();
	IMpoServer *pServer = srv.get();

	pServer->Initialize(uPort, "127.0.0.1");

	IMpoClientSPtr cli = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = cli.get();
	net_result res = pClient->connect_and_wait("127.0.0.1", uPort);
	TEST_CHECK(res == NET_OK);

	// accept connection
	mpo_sockpres_autoptr pres = pServer->Accept(5000);
	TEST_REQUIRE(pres.get());
	TEST_CHECK_EQUAL(pres->GetIPString(), "127.0.0.1");

	res = pClient->wait_connect(5000);
	TEST_CHECK(res == NET_OK);

	pres = pClient->get_socket_safe();
	TEST_CHECK_EQUAL(pres->GetPort(), uPort);
	string strIP = pres->GetIPString();

	// workaround WINE quirky behavior
	TEST_CHECK((strIP  ==  "127.0.0.1") || (strIP == "127.12.34.56"));
}
