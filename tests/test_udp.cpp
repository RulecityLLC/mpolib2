#include "test_headers.h"

void test_udp_broadcast()
{
	MpoUdpReceiverSPtr RecvSPtr = MpoUdpReceiver::GetInstance(4756);
	MpoUdpReceiver *pRecv = RecvSPtr.get();
	TEST_REQUIRE(pRecv);

	MpoUdpSenderSPtr SendSPtr = MpoUdpSender::GetInstance(4756, "127.0.0.1", true);
	MpoUdpSender *pSend = SendSPtr.get();
	TEST_REQUIRE(pSend);

	string strPayload = "hi there, ya sick frecks!";
	bool bRes = pSend->SendPacket(strPayload);
	TEST_CHECK(bRes);

	string strResult, strIP;
	net_result res = pRecv->RecvPacket(strResult, strIP, 1000);	// give up to 1 second to receive this UDP packet, OSX sometimes misses it
	TEST_REQUIRE_EQUAL(NET_OK, res);

	TEST_CHECK_EQUAL("127.0.0.1", strIP);

	TEST_CHECK_EQUAL(strResult, strPayload);
}

TEST_CASE(udp_broadcast)
{
	test_udp_broadcast();
}
