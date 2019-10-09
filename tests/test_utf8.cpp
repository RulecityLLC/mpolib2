#include "test_headers.h"

void test_utf8_1()
{
	mpo_wstring wstrNormal;
	bool bRes = false;
	string s;

	wstrNormal.push_back('A');

	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(bRes);

	TEST_CHECK_EQUAL("A", s);

	wstrNormal.clear();
	wstrNormal.push_back(0x99);
	wstrNormal.push_back(0xA9);
	bRes = mpom::ToUTF8(s, wstrNormal);	// 0x99 0xa9
	TEST_CHECK(bRes);

	unsigned char res1[] = { 0xC2, 0x99, 0xC2, 0xA9 };
	TEST_CHECK_EQUAL(4, s.size());
	string sCmp = string((char *) res1, sizeof(res1));
	TEST_CHECK_EQUAL(sCmp, s);

	// try an out of range string
	wstrNormal.clear();
	wstrNormal.push_back(0xD800);

	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(!bRes);

	// try an out of range string
	wstrNormal.clear();
	wstrNormal.push_back(0xDFFF);

	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(!bRes);

	// barely in range
	wstrNormal.clear();
	wstrNormal.push_back(0xD7FF);

	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(bRes);

	// barely in range
	wstrNormal.clear();
	wstrNormal.push_back(0xE000);

	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(bRes);

	// 3 octets
	wstrNormal.clear();
	wstrNormal.push_back(0x2122);
	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(bRes);

	mpo_wstring ws;
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(bRes);
	TEST_CHECK(wstrNormal == ws);
	
	// 4 octets
	wstrNormal.clear();
	mpo_wchar_t ch = 0x10FFFF;
	assert(sizeof(mpo_wchar_t) == 4);
	assert(ch == 0x10FFFF);
	wstrNormal.push_back(ch);
	bRes = mpom::ToUTF8(s, wstrNormal);
	TEST_CHECK(bRes);

	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(bRes);
	TEST_CHECK(wstrNormal == ws);
}

TEST_CASE(utf8_1)
{
	test_utf8_1();
}

void test_utf8_2()
{
	string s;
	mpo_wstring ws;
	bool bRes = false;

	// try an empty string
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(bRes);

	unsigned char res1[] = { 0xC2, 0x99, 0xC2, 0xA9 };
	string sCmp = string((char *) res1, sizeof(res1));

	bRes = mpom::FromUTF8(ws, sCmp);
	TEST_CHECK(bRes);

	TEST_REQUIRE_EQUAL(2, ws.size());

	TEST_CHECK(0x99 == ws[0]);
	TEST_CHECK(0xA9 == ws[1]);

	// try codes the RFC warns us about
	s.clear();
	s.push_back((char) 0xC0);
	s.push_back((char) 0x80);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	s.clear();
	s.push_back((char) 0xE0);
	s.push_back((char) 0x80);
	s.push_back((char) 0x80);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	s.clear();
	s.push_back((char) 0xF0);
	s.push_back((char) 0x80);
	s.push_back((char) 0x80);
	s.push_back((char) 0x80);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	// invalid octets
	s.clear();
	s.push_back((char) 0xC0);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	// invalid octets
	s.clear();
	s.push_back((char) 0xC1);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	// invalid octets
	s.clear();
	s.push_back((char) 0xFF);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	// invalid octets
	s.clear();
	s.push_back((char) 0xF5);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(!bRes);

	// surrogate pairs (from RFC)
	s.clear();
	// ED A1 8C ED BE B4
	s.push_back((char) 0xEd);
	s.push_back((char) 0xA1);
	s.push_back((char) 0x8C);
	s.push_back((char) 0xED);
	s.push_back((char) 0xBE);
	s.push_back((char) 0xB4);
	bRes = mpom::FromUTF8(ws, s);
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(2, ws.size());
	TEST_CHECK(0xd84c == ws[0]);
	TEST_CHECK(0xdfb4 == ws[1]);

}

TEST_CASE(utf8_2)
{
	test_utf8_2();
}

void test_utf8_3()
{
	mpo_wstring wstrEmpty;

	string s = mpom::ToUTF8Ex(wstrEmpty);
	wstring wstrDst = mpom::FromUTF8ExW(s);
}

TEST_CASE(utf8_3)
{
	test_utf8_3();
}