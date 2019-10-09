#include "test_headers.h"

void test_nat_stream1()
{
	MockMpoTimer mockTimer;
	MockMpoUdp mockUdp;
	MockMpoUdpFactory mockUdpFactory;
	string strRendezHost = "localhost";
	unsigned int uRendezPort = 6666;
	unsigned int uSrcPort = 7777;
	string strDstHost = "10.0.0.1";
	unsigned int uDstPort = 8888;
	unsigned int uRetryInterval = 1000;
	unsigned int uDstPubPort = 9999;
	unsigned int uMsTilAttempt = 20000;

	unsigned char arrPayload[20] = {
		'1', '3', '5', '1',	// version ID
		0, 0, 0, 0,	// local IP
		0x1E, 0x61,	// source port
		10, 0, 0, 1,	// target IPv4
		0x22, 0xB8,	// destination port (behind firewall)
		0, 0, 0, 0	// connection ID
	};
	string strPayload = string((char *) arrPayload, sizeof(arrPayload));

	unsigned char arrPayloadResponse[] = {
		'3', '3', '5', '1',	// version ID
		0, 0, 0, 0,	// connection ID
		10, 0, 0, 1,	// target IPv4
		0x27, 0x0F,		// target internet port
		0, 0, 0x4E, 0x2E,	// ms until next syncronized connection attempt
		0, 0, 0x3A, 0x98,	// ms until rendez server may be contacted again
	};
	string strPayloadResponse = string((char *) arrPayloadResponse, sizeof(arrPayloadResponse));

	EXPECT_CALL(mockTimer, GetCurValMs())
		.WillOnce(Return(1))
		.WillOnce(Return(2000));
	EXPECT_CALL(mockTimer, GetElapsedMs(0))
		.WillOnce(Return(0))	// first time simulate no time having elapsed
		.WillOnce(Return(1001));

	EXPECT_CALL(mockUdpFactory, GetInstance(uSrcPort, false))
		.WillOnce(Return(MockMpoUdp::ToSPtr(&mockUdp)));

	EXPECT_CALL(mockUdp, RecvPacket( _ ))
		.WillOnce(Return(NET_TIMEOUT))
		.WillOnce(Return(NET_TIMEOUT))
		.WillOnce(DoAll(SetArgumentPointee<0>(strPayloadResponse), Return(NET_OK)));
	EXPECT_CALL(mockUdp, SendPacketToEx(strPayload, "127.0.0.1", uRendezPort));	// should communicate with rendez server

	IMpoNatStreamClientSPtr NatSPtr = MpoNatStreamClient::GetInstance(&mockUdpFactory, &mockTimer, strRendezHost, uRendezPort, uSrcPort);
	IMpoNatStreamClient *pNat = NatSPtr.get();
	TEST_REQUIRE(pNat);

	unsigned int uID;
	bool bRes = false;

	bRes = pNat->StartConnect(&uID, strDstHost.c_str(), uDstPort, uRetryInterval);
	TEST_REQUIRE(bRes);

	net_result res;
	res = pNat->CheckConnect(uID);
	TEST_REQUIRE_EQUAL(NET_TIMEOUT, res);

	// this time, send request to rendez server
	res = pNat->CheckConnect(uID);
	TEST_REQUIRE_EQUAL(NET_TIMEOUT, res);

	// now get response from rendez server
	res = pNat->CheckConnect(uID);
	TEST_REQUIRE_EQUAL(NET_TIMEOUT, res);

}

TEST_CASE(nat_stream1)
{
//	test_nat_stream1();
}
