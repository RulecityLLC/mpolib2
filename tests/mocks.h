#ifndef MOCKS_H
#define MOCKS_H

#include <gmock/gmock.h>

using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgumentPointee;

// use this to mock shared pointers
class MockStubDeleter
{
protected:

		// does nothing!  (prevents mock object from being deleted when we don't want it to be)
        class stub_deleter
        {
        public:
                void operator()(MockStubDeleter *p) { /* does nothing */ }
        };
};

class MockMpoUdpFactory : public IMpoUdpFactory
{
public:
	MOCK_METHOD2(GetInstance, IMpoUdpSPtr(unsigned int uSrcPort, bool bBroadcast));
};

class MockMpoUdp : public IMpoUdp, public MockStubDeleter
{
public:
	static IMpoUdpSPtr ToSPtr(MockMpoUdp *pInstance) { return IMpoUdpSPtr(pInstance, MockMpoUdp::stub_deleter()); }

	MOCK_METHOD3(SendPacketToEx, void(const string &, const string &, unsigned int));

	MOCK_METHOD1(RecvPacket, net_result(string *));
};

class MockMpoTimer : public IMpoTimer
{
public:
	MOCK_METHOD0(GetCurValMs, unsigned int());
	MOCK_METHOD1(GetElapsedMs, unsigned int(unsigned int));
};

#endif // MOCKS_H
