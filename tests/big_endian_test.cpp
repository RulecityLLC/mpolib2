#include "test_headers.h"

TEST_CASE(big_endian_conv)
{
	unsigned char buf[] = { 0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

	unsigned int u = 0;
	MPO_UINT64 u64 = 0;
	unsigned int uNum = 0x112233;
	MPO_UINT64 u64Num = 0x11223344556677ULL;

	// buffer to variable
	u = mpom::bige2uint32(buf);
	TEST_CHECK_EQUAL(u, uNum);

	u64 = mpom::bige2uint64(buf);
	TEST_CHECK(u64 == u64Num);

	// now test variable to buffer
	string strRes = mpom::uint2bige32(uNum);
	int i = memcmp(strRes.data(), buf, 4);
	TEST_CHECK_EQUAL(i, 0);

	strRes = mpom::uint2bige64(u64Num);
	i = memcmp(strRes.data(), buf, 8);
	TEST_CHECK_EQUAL(i, 0);
}
