/*
 * tests\main.cpp
 *
 * Copyright (C) 2005 Matthew P. Ownby
 *
 * This file is part of MPOLIB, a multi-purpose library
 *
 * MPOLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPOLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MPO's NOTE:
//  I may wish to use MPOLIB in a proprietary product some day.  Therefore,
//   the only way I can accept other people's changes to my code is if they
//   give me full ownership of those changes.

#include "test_headers.h"

#ifndef WIN32
#include <signal.h>
#endif

using namespace std;

bool g_bDoSlowTests = true;
bool g_bRunningUnderWine = false;

// returns true if connection succeeded
bool test_connect(const char *ipv4, unsigned short port)
{
	bool result = false;
	cout << "Trying to connect to " << ipv4 << ", port " << port << "..." << endl;

	IMpoClientSPtr client = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = client.get();
	net_result res = pClient->connect_to_host(ipv4, port, 1000);

	// try to connect for a while ...
	for (int i = 0; (i < 5) && (res == NET_TIMEOUT); i++)
	{
		cout << "No connection yet, continuing to try ..." << endl;
		res = pClient->wait_connect(1000);
	}

	if (res == NET_OK) result = true;

	client.reset();

	return result;
}

// firewall just drops us
TEST_CASE(blockedport)
{
    if (!g_bDoSlowTests)
    {
        return;
    }

	// www.hotmail.com, port 81 is dropped
	bool result = test_connect("207.68.172.239", 81);
	TEST_CHECK(!result);	// test passes if test_connect fails
}

// we're accepted quickly
TEST_CASE(goodport)
{
    if (!g_bDoSlowTests)
    {
        return;
    }

	bool result = test_connect("www.daphne-emu.com", 80);
	TEST_CHECK(result);
}

// we're rejected quickly
TEST_CASE(closedport)
{
    if (!g_bDoSlowTests)
    {
        return;
    }

	bool result = test_connect("127.0.0.1", 81);
	TEST_CHECK(!result);
}

// returns true if parsing succeeded
bool url_helper(const string &url, const string &expected_host, const unsigned int &expected_port,
				const string &expected_uri)
{
	string host = "", uri = "";
	unsigned int port = 0;
	bool result = false;

	if (net_parse_url(url, host, port, uri) && (host == expected_host)
		&& (uri == expected_uri) && (port == expected_port))
	{
		result = true;
	}
	else
	{
		// uncomment for debugging
		/*
		cout << url << " --> FAILED!" << endl;
		cout << host << ", expected " << expected_host << endl;
		cout << numstr::ToStr(port) << ", expected " << numstr::ToStr(expected_port) << endl;
		cout << uri << ", expected " << expected_uri << endl;
		*/
	}
	return result;
}

TEST_CASE(urls)
{
	string url1 = "http://www.cnn.com:1025/index.html";
	string url2 = "http://www.cnn.com:/";
	string url3 = "http://www.cnn.com:";
	string url4 = "http://www.cnn.com";
	string url5 = "http://www.cnn.com:7";
	string url6 = "http://www.daphne-emu.com/files/ramstuff.zip";
	string bad_url = "www.cnn.com";

	TEST_CHECK(url_helper(url1, "www.cnn.com", 1025, "/index.html"));
	TEST_CHECK(url_helper(url2, "www.cnn.com", 80, "/"));
	TEST_CHECK(url_helper(url3, "www.cnn.com", 80, "/"));
	TEST_CHECK(url_helper(url4, "www.cnn.com", 80, "/"));
	TEST_CHECK(url_helper(url5, "www.cnn.com", 7, "/"));
	TEST_CHECK(url_helper(url6, "www.daphne-emu.com", 80, "/files/ramstuff.zip"));
	TEST_CHECK(!url_helper(bad_url, "www.cnn.com", 80, "/"));
}

TEST_CASE(hex)
{
	// string to number
	unsigned int u = numstr::ToUint32("20", 16);
	TEST_CHECK((u == 32));

	u = numstr::ToUint32("abcd", 16);
	TEST_CHECK((u == 43981));

	u = numstr::ToUint32("b0b0", 16);
	TEST_CHECK((u == 45232));

	u = numstr::ToUint32("Ae04", 16);
	TEST_CHECK((u == 44548));

	// number to string
	TEST_CHECK(mpom::str_case_eq(numstr::ToStr(44548, 16, 0),"ae04"));
	TEST_CHECK(mpom::str_case_eq(numstr::ToStr(43981, 16, 6),"00abcd"));
	TEST_CHECK(mpom::str_case_eq(numstr::ToStr(45232, 16, 3),"b0b0"));
}

TEST_CASE(case)
{
	TEST_CHECK(mpom::str_toupper("hi az") == "HI AZ");
	TEST_CHECK(mpom::str_tolower("HI AZ") == "hi az");
}

TEST_CASE(http)
{
    if (!g_bDoSlowTests)
    {
        return;
    }

	cout << "Trying to download from www.daphne-emu.com ..." << endl;

	bool test_result = false;
	StreamMsg res;
	net_result netres;
	string host = "";
	unsigned int port = 0;
	string uri = "";
	IMpoHttpSPtr hptr;
	IMpoHttp *h = NULL;
	nonblocking_sharedptr pStream;

	test_result = false;	// redundant, but put here for clarity
	IMpoClientSPtr client = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = client.get();
	netres = pClient->connect_and_wait("www.daphne-emu.com", 80);

	if (netres == NET_OK)
	{
		pStream = pClient->get_stream();
		TEST_REQUIRE(pStream.get() != 0);
		hptr = MpoHttpFactory::CreateInstance(pStream.get(), "test");
		h = hptr.get();
		TEST_REQUIRE(h != 0);

		// TEST #1 .. download a small, known to be good file
		cout << "Starting HTTP download test..." << endl;
		net_parse_url("http://www.daphne-emu.com/download/ramstuff.zip", host, port, uri);

		// test the 'HEAD' functionality while we're at it
		h->StartHead(host, uri, false, 5000);
		// process all the headers
		do
		{
			res = h->GetHeaders(100);
		} while (res == MSG_TIMEOUT);
		string strLastModified = h->GetLastModified();
		cout << "Last modified is: " << strLastModified << endl;

		h->StartGet(host, uri, false, 5000);

		// process all the headers
		do
		{
			res = h->GetHeaders(100);
		} while (res == MSG_TIMEOUT);

		// if the file in question is available for the taking ...
		if ((res == MSG_OK) && (h->GetContentLength() > 0))
		{
			string buf = "";

			if ((h->FillBuf(buf, 5000) == MSG_END) && (h->GetContentLength() == buf.size()))
			{
				// if MD5 matches
				if (mpom::str_case_eq(mpom::compute_md5((const unsigned char *) buf.data(), (unsigned int) buf.size()),
					"45A88B59D3B350EE085D0AFE140E1F03"))
				{
					cout << "MD5 APPEARS CORRECT!" << endl;

					string strLastModified2 = h->GetLastModified();

					// make sure HEAD and GET match up
					if (strLastModified2 == strLastModified)
					{
						// test "304 Not Modified" functionality
						h->StartGet(host, uri, false, 5000, strLastModified2);
						// process all the headers
						do
						{
							res = h->GetHeaders(100);
						} while (res == MSG_TIMEOUT);

						// it shouldn't have resent it to us
						if ((res == MSG_OK) && (h->GetResultCode() == 304))
						{
							test_result = true;
						}
						else
						{
							cerr << "Result code wasn't 304 as expected!" << endl;
						}
					}
					else
					{
						cerr << "LastModified times differ!" << endl;
					}
				}
				else cout << "MD5 IS INCORRECT!" << endl;
			}
			else
			{
				cerr << "Couldn't fill the buffer..." << endl;
			}
		}
		else cout << "Error starting download..." << endl;

	} // end test #1
	else cout << "ERROR : connection failed for TEST #1" << endl;

	TEST_CHECK(test_result);

	test_result = false;	// reset
	netres = pClient->connect_and_wait("www.daphne-emu.com", 80);
	if (netres == NET_OK)
	{
		pStream = pClient->get_stream();
		TEST_REQUIRE(pStream != 0);
		hptr = MpoHttpFactory::CreateInstance(pStream.get(), "test");
		h = hptr.get();
		TEST_REQUIRE(h != 0);

		// TEST #3, try downloading a file that doesn't exist
		cout << "Starting HTTP invalid file test..." << endl;
		h->StartGet(host, "/dummy/fool", false, 5000);

		// process all the headers
		do
		{
			res = h->GetHeaders(100);
		} while (res == MSG_TIMEOUT);

		if (res == MSG_OK)
		{
			if (h->GetResultCode() == 404)
			{
				cout << "GOOD : non-existent file download failed as expected" << endl;
				test_result = true;
			}
			else
			{
				cout << "Got unexpected result code: " << h->GetResultCode() << endl;
			}
		}
		else
		{
			cout << "ERROR : non-existent file download did not fail!??" << endl;
		}

	}
	TEST_CHECK(test_result);

}

TEST_CASE(http2)
{
	bool test_result = false;
	nonblocking_sharedptr pStream;
	IMpoHttp *h = NULL;
	IMpoHttpSPtr hptr;
	StreamMsg msg;

    if (!g_bDoSlowTests)
    {
        return;
    }

	cout << "Trying to connect to www.d-l-p.com..." << endl;

    IMpoClientSPtr client = MpoClientFactory::CreateInstance();
    IMpoClient *pClient = client.get();
	net_result res = pClient->connect_and_wait("www.d-l-p.com", 80);
	if (res == NET_OK)
	{
		pStream = pClient->get_stream();
		TEST_REQUIRE(pStream.get() != 0);
		hptr = MpoHttpFactory::CreateInstance(pStream.get(), "test");
		h = hptr.get();
		TEST_REQUIRE(h != 0);

		// TEST #1, grabbing from IIS
		h->StartHead("www.d-l-p.com", "/", true, 5000);	// changed from GET to HEAD so we get disconnected immediately

		// process all the headers
		do
		{
			msg = h->GetHeaders(100);
		} while (msg == MSG_TIMEOUT);

		if (msg == MSG_OK)
		{
			cout << "GOOD : we got something!" << endl;
			test_result = true;
		}
		else cout << "ERROR : Didn't like that URI..." << endl;

	}
	else cout << "connection failed :(" << endl;
	TEST_CHECK(test_result);

	// TEST #2, trying to grab when we're actually disconnected
	test_result = false;
	h->StartHead("www.d-l-p.com", "/", true, 10000);

	// process all the headers
	do
	{
		msg = h->GetHeaders(100);
	} while (res == NET_TIMEOUT);

	if ((msg == MSG_END) || (msg == MSG_ERROR))
	{
		cout << "GOOD : we were disconnected" << endl;
		test_result = true;
	}
	else cout << "ERROR : didn't get disconnect notice" << endl;
	TEST_CHECK(test_result);
}

TEST_CASE(bignums)
{
	MPO_UINT64 big_num = 1;
#ifdef WIN32
	MPO_UINT64 big_num2 = 85899345920;
	MPO_INT64 big_num3 = -85899345920;
#else
	// GCC-3 is retarded and makes us put ULL at the end
	MPO_UINT64 big_num2 = 85899345920ULL;
	MPO_INT64 big_num3 = -85899345920LL;
#endif
	big_num <<= 33;	// go beyond 32-bit range to 8589934592
	string test = "";

	test = numstr::ToStr(big_num);
	TEST_CHECK_EQUAL(test, "8589934592");

	test = "85899345920";
	big_num = numstr::ToUint64(test.c_str());
	TEST_CHECK_EQUAL(big_num, big_num2);

	test = numstr::ToStr(big_num3);
	TEST_CHECK_EQUAL(test, "-85899345920");

	// make sure framework can handle uint64 and integer
	big_num = 1;
	TEST_CHECK_EQUAL(big_num, 1);
}

const int MULTI_NUM = 254;

TEST_CASE(unit_conversion)
{
	TEST_CHECK((numstr::ToUnitStr(993) == "993 B"));
	TEST_CHECK((numstr::ToUnitStr(1600) == "1.56 KiB"));
	TEST_CHECK((numstr::ToUnitStr(1049076) == "1.00 MiB"));
	TEST_CHECK((numstr::ToUnitStr(1073741829) == "1.00 GiB"));
	TEST_CHECK((numstr::ToUnitStr(1637) == "1.60 KiB"));
	TEST_CHECK((numstr::ToUnitStr(1676673) == "1.60 MiB"));
	TEST_CHECK((numstr::ToUnitStr(1716913176) == "1.60 GiB"));
}

TEST_CASE(time_conversion)
{
	TEST_CHECK((SToStr(3456000) == "40 days"));
	TEST_CHECK((SToStr(6480000) == "75 days"));
	TEST_CHECK((SToStr(6534000) == "75 days, 15 hours"));
}

#ifdef HAVE_ZLIB_H

TEST_CASE(unzip)
{
	bool bPassed = true;
	mpo_unzip uz;
	if (uz.open("test_mpolib_dummy.zip"))
	{
		mpo_buf buf;
		if (uz.read_file("dummy.txt", buf))
		{
			if (mpom::str_case_eq(mpom::compute_md5((const unsigned char *) buf.data(), (unsigned int) buf.size()),
				"a726c99789f4dacc4885a3b4fc5101d9"))
			{
				bPassed = true;
			}
		}
	}

	TEST_CHECK(bPassed);

	bool bTestAll = uz.test_all(NULL);
	TEST_CHECK(bTestAll);

	string strErrMsg = "";
	bool bExtractAll = uz.extract_all("blah", strErrMsg, NULL);
	TEST_CHECK(bExtractAll);
}

#endif // zlib

TEST_CASE(mpobuf)
{
	mpo_buf buf1, buf2;
	string s = "blah!";
	const char *cpszBunk = "This is bunk!";

	buf1 = s;
	TEST_CHECK(memcmp(buf1.data(), s.data(), s.size()) == 0);

	buf2 = cpszBunk;
	TEST_CHECK(memcmp(buf2.data(), cpszBunk, strlen(cpszBunk)) == 0);

	buf1 = buf2;
	TEST_CHECK(memcmp(buf1.data(), buf2.data(), buf1.size()) == 0);

	// test == operator
	TEST_CHECK(buf1 == buf2);

	buf1 = s;	// make buf1 != buf2
	TEST_CHECK(buf1 != buf2);
}

TEST_CASE(filepath)
{
	string path1 = "c:\\temp\\blah.txt";
	string path2 = "/mnt/whatever/blah.txt";
	string path3 = "blah.txt";
	string path4 = "";
	TEST_CHECK((mpom::get_file_from_path(path1) == "blah.txt"));
	TEST_CHECK((mpom::get_file_from_path(path2) == "blah.txt"));
	TEST_CHECK((mpom::get_file_from_path(path3) == "blah.txt"));
	TEST_CHECK((mpom::get_file_from_path(path4) == ""));

	// wide version
	TEST_CHECK((mpom::get_file_from_path(mpom::str_conv(path1)) == L"blah.txt"));
}

TEST_CASE(bigstr)
{
	wstring strBig = L"Big!";
	string strNormal = "Hi!";
	strBig = mpom::str_conv(strNormal);
	TEST_CHECK((strBig == L"Hi!"));

	strNormal = "Blah";
	strBig = mpom::str_conv (strNormal);
	TEST_CHECK((strBig == L"Blah"));

	strBig = L"Big Guy!";
	strNormal = mpom::str_conv(strBig);
	TEST_CHECK((strNormal == "Big Guy!"));

	wstring str = L"HUGE GUY!";
	strNormal = mpom::str_conv(str.c_str());
	TEST_CHECK((strNormal == "HUGE GUY!"));

	// 	wstring wstrTricky = L"Matt Ownby�s iPod";

	// this string can't be properly converted to multi-byte, so we make sure we can fallback to something sensible
	wstring wstrTricky = L"Matt Ownby";
	wstrTricky += (wchar_t) 0x2019;	// this is an apostrophe
	wstrTricky += L"s iPod";
	strNormal = mpom::str_conv(wstrTricky);

	// wine's windows DLL has a bug which causes this test to fail, so don't run this test if running under wine
	if (!g_bRunningUnderWine)
	{
		TEST_CHECK((strNormal == "Matt Ownbys iPod"));
	}
}

TEST_CASE(path)
{
	TEST_CHECK((mpom::standardize_path("./abc") == "abc"));
    TEST_CHECK((mpom::standardize_path("/abc") == "/abc"));
    TEST_CHECK((mpom::standardize_path("abc") == "abc"));
    TEST_CHECK((mpom::standardize_path("./abc/./hi") == "abc/hi"));
    TEST_CHECK((mpom::standardize_path("abc//.///hi") == "abc/hi"));
    TEST_CHECK((mpom::standardize_path("./abc\\hi") == "abc/hi"));
    TEST_CHECK((mpom::standardize_path(".//abc/hi/.") == "abc/hi"));
    TEST_CHECK((mpom::standardize_path("abc.def") == "abc.def"));
    TEST_CHECK((mpom::standardize_path("abc.") == "abc."));
    TEST_CHECK((mpom::standardize_path("abc/def/hij") == "abc/def/hij"));
    TEST_CHECK((mpom::standardize_path(".") == "."));
}

void test_escape()
{
    TEST_CHECK((escape_arg("hi")=="hi"));
    TEST_CHECK((escape_arg("hi there")=="\"hi there\""));
    TEST_CHECK((escape_arg("hi\"there")=="hithere"));
    TEST_CHECK((escape_arg("\"hi\" \"there\"")=="\"hi there\""));
}

TEST_CASE(text_parsing)
{
	string s1 = " hi there ";
    TEST_CHECK((mpom::get_first_word(s1) == "hi") && (s1 == " there "));
	s1 = " hi there ";
    TEST_CHECK((mpom::get_first_line(s1) == " hi there ") && (s1.empty()));

	string s2 = "hi\nthere";
    TEST_CHECK((mpom::get_first_word(s2) == "hi") && (s2 == "\nthere"));
	s2 = "hi\nthere";
    TEST_CHECK((mpom::get_first_line(s2) == "hi") && (s2 == "there"));

	string s3 = "hi\rthere";
    TEST_CHECK((mpom::get_first_word(s3) == "hi") && (s3 == "\rthere"));
	s3 = "hi\rthere";
    TEST_CHECK((mpom::get_first_line(s3) == "hi") && (s3 == "there"));

	string s4 = "hi\r\nthere";
    TEST_CHECK((mpom::get_first_word(s4) == "hi") && (s4 == "\r\nthere"));
	s4 = "hi\r\nthere";
    TEST_CHECK((mpom::get_first_line(s4) == "hi") && (s4 == "there"));

	string s5 = "hi\n\r there";
    TEST_CHECK((mpom::get_first_word(s5) == "hi") && (s5 == "\n\r there"));
	s5 = "hi\n\r there";
    TEST_CHECK((mpom::get_first_line(s5) == "hi") && (s5 == " there"));

	string s6 = "";
    TEST_CHECK((mpom::get_first_word(s6) == "") && (s6 == ""));
	s6 = "";
    TEST_CHECK((mpom::get_first_line(s6) == "") && (s6 == ""));

	string s7 = "\n\r\t";
    TEST_CHECK((mpom::get_first_word(s7) == "") && (s7 == "\n\r\t"));
	s7 = "\n\r\t";
    TEST_CHECK((mpom::get_first_line(s7) == "") && (s7 == "\t"));

}

TEST_CASE(test_dummy_stream)
{
	char buf[] = "this is a test...";
	char buf2[80];
	size_t stBytesRead = 0;

	nonblocking_sharedptr pStream = NonblockingStreamTester::GetInstance(buf, sizeof(buf));
	TEST_REQUIRE(pStream.get() != 0);

	stBytesRead = pStream->Read(buf2, sizeof(buf2), 0);
	TEST_CHECK(stBytesRead == sizeof(buf));

	int i = memcmp(buf2, buf, sizeof(buf));
	TEST_CHECK_EQUAL(i, 0);

	// make sure 
	stBytesRead = pStream->Read(buf2, sizeof(buf2), 0);
	TEST_CHECK_EQUAL(stBytesRead, 0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

	// if an argument was specified
	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			string sArg = argv[i];

			// if they are running under wine
			if (sArg == "-wine")
			{
				g_bRunningUnderWine = true;
			}
			else if (sArg == "-quick")
			{
				g_bDoSlowTests = false;
			}
			else
			{
				fprintf(stderr, "Unknown command line argument: %s\n", sArg.c_str());
				return 1;
			}
		}
	}

	net_init();

	int iResult = RUN_ALL_TESTS();

	net_shutdown();

	return iResult;
}
