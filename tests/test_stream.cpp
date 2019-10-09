#include "test_headers.h"

void test_stream_helper1(blocking_sharedptr pStream, char *buf, size_t stBufSize)
{
	unsigned char dstbuf[80];
	size_t stRes = 0;
	memset(dstbuf, 0, sizeof(dstbuf));
	MPO_INT64 i64BufSize = stBufSize;
	char ch = 'A';

	TEST_REQUIRE(pStream.get());
	TEST_CHECK(pStream->CanRead());
	TEST_CHECK(pStream->CanSeek());
	TEST_REQUIRE(pStream->CanWrite());
	TEST_CHECK_EQUAL(pStream->GetLength(), (MPO_UINT64) stBufSize);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 0);

	// try changing the first byte
	stRes = pStream->Write(&ch, 1);
	TEST_CHECK_EQUAL(stRes, 1);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 1);
	// read next byte to make sure it's correct
	stRes = pStream->Read(dstbuf, 1);	// read back to see if the byte 'took'
	TEST_REQUIRE_EQUAL(dstbuf[0], buf[1]);
	pStream->Seek(0, MPO_SEEK_SET);
	stRes = pStream->Read(dstbuf, 1);	// read back to see if the byte 'took'
	TEST_CHECK_EQUAL(dstbuf[0], ch);
	// now change it back
	pStream->Seek(0, MPO_SEEK_SET);
	pStream->Write(buf, 1);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 1);
	pStream->Seek(0, MPO_SEEK_SET);
	// subsequent tests will verify whether this worked

	stRes = pStream->Read(dstbuf, 1);
	TEST_REQUIRE_EQUAL(dstbuf[0], buf[0]);
	TEST_CHECK_EQUAL(stRes, 1);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 1);

	stRes = pStream->Read(dstbuf, 2);
	TEST_REQUIRE_EQUAL(dstbuf[0], buf[1]);
	TEST_CHECK_EQUAL(dstbuf[1], buf[2]);
	TEST_CHECK_EQUAL(stRes, 2);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 3);

	// try to read too far
	stRes = pStream->Read(dstbuf, stBufSize);
	TEST_CHECK_EQUAL(dstbuf[0], buf[3]);
	TEST_CHECK_EQUAL(dstbuf[1], buf[4]);
	TEST_CHECK_EQUAL(stRes, stBufSize - 3);
	TEST_CHECK_EQUAL(pStream->GetPosition(), (MPO_UINT64) stBufSize);

	// test seeking
	bool bRes;
	// Try to seek forward when we're already at the end of the stream.
	// This should be allowed, but reading passed the end of the stream is not.
	bRes = pStream->Seek(1, MPO_SEEK_CUR);
	TEST_REQUIRE(bRes);

	// make sure we can't read
	stRes = pStream->Read(dstbuf, 1);
	TEST_REQUIRE(stRes == 0);

	// seek backward 2 bytes (so that we're 1 away from the last byte in the stream)
	bRes = pStream->Seek(-2, MPO_SEEK_CUR);
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), (MPO_UINT64) (stBufSize - 1));

	// seek to the end
	bRes = pStream->Seek(0, MPO_SEEK_END);
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), (MPO_UINT64) stBufSize);

	// seek to the end - 1
	bRes = pStream->Seek(-1, MPO_SEEK_END);
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), (MPO_UINT64) (stBufSize - 1));

	// try to read 2 bytes, make sure we can only read 1
	stRes = pStream->Read(dstbuf, 2);
	TEST_REQUIRE(stRes == 1);

	// seek to the end + 1
	bRes = pStream->Seek(1, MPO_SEEK_END);
	TEST_REQUIRE(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), (MPO_UINT64) (stBufSize + 1));

	// make sure we can't read
	stRes = pStream->Read(dstbuf, 1);
	TEST_REQUIRE(stRes == 0);

	// seek to the very beginning from the end
	bRes = pStream->Seek(-i64BufSize, MPO_SEEK_END);
	TEST_REQUIRE(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 0);

	// seek one further than we're allowed, starting from the end
	bRes = pStream->Seek(-(i64BufSize+1), MPO_SEEK_END);
	TEST_CHECK(!bRes);

	// seek to beginning of file
	bRes = pStream->Seek(0, MPO_SEEK_SET);
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 0);

	// seek backward 1 byte (shouldn't be allowed to)
	bRes = pStream->Seek(-1, MPO_SEEK_CUR);
	TEST_CHECK(!bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 0);

	// seek to beginning of the file - 1
	bRes = pStream->Seek(-1, MPO_SEEK_SET);
	TEST_REQUIRE(!bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 0);

	// seek to beginning of the file + 1
	bRes = pStream->Seek(1, MPO_SEEK_SET);
	TEST_CHECK(bRes);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 1);

	// test reading once again to make sure we're where we should be
	stRes = pStream->Read(dstbuf, 2);
	TEST_CHECK_EQUAL(dstbuf[0], buf[1]);
	TEST_CHECK_EQUAL(dstbuf[1], buf[2]);
	TEST_CHECK_EQUAL(stRes, 2);
	TEST_CHECK_EQUAL(pStream->GetPosition(), 3);

}

TEST_CASE(memstream1)
{
	char buf[] = "this is a test";
	blocking_sharedptr pStream = MpoMemStreamFactory::CreateInstance(buf, sizeof(buf));

	test_stream_helper1(pStream, buf, sizeof(buf));
	TEST_CHECK(pStream->CanWrite());
}

TEST_CASE(filestream1)
{
	char buf[] = "this is a test";
	const char *filename = "test.bin";
	blocking_sharedptr pStream = MpoFileStreamFactory::CreateInstance(filename, MPO_OPEN_CREATE);
	TEST_REQUIRE(pStream.get());
	TEST_CHECK(pStream->CanWrite());
	size_t stRes = 0;

	stRes = pStream->Write(buf, sizeof(buf));
	TEST_CHECK_EQUAL(stRes, sizeof(buf));

	pStream.reset();
	pStream = MpoFileStreamFactory::CreateInstance(filename, MPO_OPEN_READWRITE);
	test_stream_helper1(pStream, buf, sizeof(buf));

	pStream.reset();

	IMpoFileIOSPtr fileio = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileio.get();

	// cleanup
	pFileIO->Delete(mpom::str_conv(filename));
}

void test_write_helper2(blocking_sharedptr pStream)
{
	char ch = 'A';
	char buf[] = "testing";
	char newbuf[80];

	// make sure stream is empty before we begin
	TEST_REQUIRE_EQUAL(pStream->GetLength(), 0);

	// TEST seeking passed end of the stream and writing
	pStream->Seek(10, MPO_SEEK_SET);
	pStream->Write(&ch, 1);
	MPO_UINT64 u64 = pStream->GetLength();
	TEST_REQUIRE_EQUAL(u64, 11);

	// TEST overwriting part of the stream and changing the length
	pStream->Seek(-1, MPO_SEEK_END);
	pStream->Write(buf, sizeof(buf));
	u64 = pStream->GetLength();
	TEST_CHECK_EQUAL(u64, 18);
	// verify results of writing
	pStream->Seek(- ((int) sizeof(buf)), MPO_SEEK_CUR);
	pStream->Read(newbuf, sizeof(buf));
	int iMemCmp = memcmp(buf, newbuf, sizeof(buf));
	TEST_REQUIRE_EQUAL(iMemCmp, 0);

	// TEST ovewriting the stream without changing the length
	pStream->Seek(0, MPO_SEEK_SET);
	for (unsigned int u = 0; u < 18; u++)
	{
		pStream->Write(&ch, 1);
	}
	u64 = pStream->GetLength();
	TEST_CHECK_EQUAL(u64, 18);

	// verify results of writing
	pStream->Seek(0, MPO_SEEK_SET);
	for (unsigned int u = 0; u < 18; u++)
	{
		pStream->Read(newbuf, 1);
		TEST_REQUIRE_EQUAL(newbuf[0], ch);
	}

	// TEST appending to the end of the stream
	pStream->Seek(0, MPO_SEEK_END);
	pStream->Write(buf, 1);
	u64 = pStream->GetLength();
	TEST_CHECK_EQUAL(u64, 19);
	// verify write
	pStream->Seek(-1, MPO_SEEK_END);
	pStream->Read(newbuf, 1);
	TEST_CHECK_EQUAL(newbuf[0], buf[0]);
}

TEST_CASE(filestream_writer)
{
	const char *filename = "test.bin";
	blocking_sharedptr pStream = MpoFileStreamFactory::CreateInstance(filename, MPO_OPEN_READWRITE);

	test_write_helper2(pStream);

	IMpoFileIOSPtr fileio = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileio.get();

	// cleanup
	pStream.reset();
	pFileIO->Delete(mpom::str_conv(filename));
}

TEST_CASE(memstream_writer)
{
	blocking_sharedptr pStream = MpoMemStreamFactory::CreateInstance();
	test_write_helper2(pStream);
}

void test_fullstream_read_helper()
{
	IMpoClientSPtr client = MpoClientFactory::CreateInstance();
	IMpoClient *pClient = client.get();
	const char *host = "www.daphne-emu.com";
	net_result res = pClient->connect_and_wait(host, 80);

	TEST_REQUIRE(res == NET_OK);

	nonblocking_sharedptr StreamSPtr = pClient->get_stream();
	INonblockingStream *pStream = StreamSPtr.get();

	// close the connection
	StreamSPtr.reset();

	unsigned char buf[8192];

	// now connect again and try to read something and let remote server disconnect on us
	res = pClient->connect_and_wait(host, 80);
	TEST_REQUIRE(res == NET_OK);

	StreamSPtr = pClient->get_stream();
	pStream = StreamSPtr.get();

	string strHttpBuf = "HEAD / HTTP/1.1\r\n"
		"Host: www.daphne-emu.com\r\n"
		"Connection: close\r\n"
		"\r\n";

	// send the HTTP request
	StreamMsg msg = StreamFull::Write(pStream, strHttpBuf.data(), strHttpBuf.size(), 5000);

	TEST_REQUIRE_EQUAL(msg, MSG_OK);

	size_t stBytesRead = 0;
	msg = StreamFull::Read(pStream, buf, sizeof(buf), stBytesRead, 5000);

	TEST_CHECK_EQUAL(MSG_OK, msg);

	// make sure other side really did disconnect
	pStream->Read(buf, sizeof(buf), 5000);
	msg = pStream->GetLastMsg();
	TEST_CHECK_EQUAL(msg, MSG_END);

	// now connect again and try to read something of unknown length but don't have remote server disconnect
	res = pClient->connect_and_wait(host, 80);
	TEST_REQUIRE(res == NET_OK);

	StreamSPtr = pClient->get_stream();
	pStream = StreamSPtr.get();

	strHttpBuf = "HEAD / HTTP/1.1\r\n"
		"Host: www.daphne-emu.com\r\n"
		"\r\n";

	// send the HTTP request
	msg = StreamFull::Write(pStream, strHttpBuf.data(), strHttpBuf.size(), 5000);

	TEST_REQUIRE_EQUAL(msg, MSG_OK);

	stBytesRead = 0;
	// short timeout period so unit test doesn't take forever
	msg = StreamFull::Read(pStream, buf, sizeof(buf), stBytesRead, 1025);

	TEST_CHECK_EQUAL(MSG_OK, msg);

	// make sure other side really did not disconnect
	// (we can use a timeout period of 0 because our previous StreamFull::Read has already timed out, so we know no data is coming anymore and that the only two possibly results here are MSG_TIMEOUT or MSG_END)
	pStream->Read(buf, sizeof(buf), 0);
	msg = pStream->GetLastMsg();
	TEST_REQUIRE_EQUAL(msg, MSG_TIMEOUT);

}

TEST_CASE(fullstream_read)
{
	test_fullstream_read_helper();
}
