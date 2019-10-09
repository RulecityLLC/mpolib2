#include "test_headers.h"

TEST_CASE(binhex)
{
	unsigned char chardata[] = { 0, 1, 0xFF, 4 };
	string strRes;

	strRes = mpom::bin2hex(chardata, sizeof(chardata));
	TEST_CHECK_EQUAL(strRes, "0001ff04");

	mpo_buf buf;
	bool bRes = mpom::hex2bin(strRes, buf);
	TEST_CHECK(bRes);

	int i = memcmp(buf.data(), chardata, sizeof(chardata));
	TEST_CHECK(i == 0);

	bRes = mpom::hex2bin("dog", buf);	// invalid hex string
	TEST_CHECK(!bRes);

}
