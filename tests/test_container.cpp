#include "test_headers.h"

TEST_CASE(container_1)
{
	blocking_sharedptr pStream = MpoMemStreamFactory::CreateInstance();
	//blocking_sharedptr pStream = MpoFileStream::GetInstance("container_test.bin", MPO_OPEN_READWRITE);
	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream.get());
	IMpoContainer *pC = pCSPtr.get();
	const char pBuf[] = "ABC";
	const char pBuf2[] = "DEFG";
	char dstbuf[80];
	bool bRes;
	size_t stRes;
	MPO_UINT64 u64;
	unsigned int id = 7, id2 = 9;	// arbitrary

	// this should fail since we have not written our header yet
	bRes = pC->StartWriteBlob(0);
	TEST_CHECK(!bRes);

	// this should fail sicne we have not written our header yet
	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(!bRes);

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	// this should fail because we can't read after we've written
	bRes = pC->ReadHeader();
	TEST_CHECK(!bRes);

	// this should fail since we shouldn't be able to write a header twice
	bRes = pC->WriteHeader();
	TEST_CHECK(!bRes);

	// since we haven't started a blob, we can't write
	stRes = pC->WriteToBlob(pBuf, sizeof(pBuf));
	TEST_REQUIRE_EQUAL(stRes, 0);

	// this should fail sicne we have not started writing a blob yet
	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(!bRes);

	u64 = pC->GetBlobIdx();
	TEST_CHECK_EQUAL(u64, 0);

	bRes = pC->StartWriteBlob(id);
	TEST_CHECK(bRes);

	stRes = pC->WriteToBlob(pBuf, sizeof(pBuf));
	TEST_CHECK_EQUAL(stRes, sizeof(pBuf));

	// should fail since we have not ended blob writing
	bRes = pC->StartWriteBlob(id2);
	TEST_REQUIRE(!bRes);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	u64 = pC->GetBlobIdx();
	TEST_CHECK_EQUAL(u64, 1);

	bRes = pC->StartWriteBlob(id2);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(pBuf2, sizeof(pBuf2));
	TEST_CHECK_EQUAL(stRes, sizeof(pBuf2));

	// close container without calling EndWriteBlob on the last blob and make sure it happens automatically
	pCSPtr.reset();

	// try opening a bad stream (invalid file) and make sure we get an error when we try to read the header
	blocking_sharedptr pStreamBadSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStreamBad = pStreamBadSPtr.get();
	pStreamBad->Write("blah", 4);

	// NOTE : we do not rewind the stream, let the container do that on its own when it's opened!
	pCSPtr = MpoContainerFactory::CreateInstance(pStreamBad);
	pC = pCSPtr.get();

	bRes = pC->ReadHeader();
	TEST_CHECK(!bRes);	// should fail

	// now open the container we just wrote and make sure we can read back what we wrote
	pCSPtr = MpoContainerFactory::CreateInstance(pStream.get());
	pC = pCSPtr.get();

	u64 = pC->GetBlobIdx();
	TEST_CHECK_EQUAL(u64, 0);

	// we added two blobs, but we have not read the header yet, so we have no way of knowing this info
	u64 = pC->GetBlobCount();
	TEST_CHECK_EQUAL(u64, 0);

	unsigned int uID = 0;

	// should fail since we haven't read the header yet
	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(!bRes);

	// try closing the blob, should fail since we have not read the header
	bRes = pC->EndReadBlob();
	TEST_REQUIRE(!bRes);

	bRes = pC->ReadHeader();
	TEST_CHECK(bRes);	// should succeed

	bRes = pC->ReadHeader();
	TEST_REQUIRE(!bRes);	// should fail, can't read twice in a row

	// this should fail since we can't write once we've read
	bRes = pC->WriteHeader();
	TEST_REQUIRE(!bRes);

	// now that we've read the header, the blob count should be correct
	u64 = pC->GetBlobCount();
	TEST_CHECK_EQUAL(u64, 2);

	// read the first blob without calling StartRead, should fail
	stRes = pC->ReadFromBlob(dstbuf, sizeof(dstbuf));
	TEST_CHECK_EQUAL(stRes, 0);

	// try closing the blob, should fail since we haven't called StartReadBlob
	bRes = pC->EndReadBlob();
	TEST_REQUIRE(!bRes);

	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(bRes);
	TEST_CHECK_EQUAL(uID, id);

	// try reading partially
	stRes = pC->ReadFromBlob(dstbuf, 2);
	TEST_CHECK_EQUAL(stRes, 2);

	// try reading the next blob when we're still in the middle of the current one
	// (should fail because we have not closed the blob)
	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(!bRes);

	// try closing the blob, should fail since we're in the middle of it
	bRes = pC->EndReadBlob();
	TEST_REQUIRE(!bRes);

	stRes = pC->ReadFromBlob(dstbuf + 2, 10);
	TEST_CHECK_EQUAL(stRes, 2);

	// try reading again, should get 0
	stRes = pC->ReadFromBlob(dstbuf, sizeof(dstbuf));
	TEST_CHECK_EQUAL(stRes, 0);

	// make sure what we read is what we wrote
	int iMemCmp = memcmp(pBuf, dstbuf, 4);
	TEST_CHECK_EQUAL(iMemCmp, 0);

	// try reading the next blob; should fail because we have not closed the current one
	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(!bRes);

	// try closing the blob, should finally succeed
	bRes = pC->EndReadBlob();
	TEST_REQUIRE(bRes);

	u64 = pC->GetBlobIdx();
	TEST_CHECK_EQUAL(u64, 1);

	// try reading the next blob; should pass
	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(bRes);

	stRes = pC->ReadFromBlob(dstbuf, 10);
	TEST_CHECK_EQUAL(stRes, 5);

	iMemCmp = memcmp(pBuf2, dstbuf, 5);
	TEST_CHECK_EQUAL(iMemCmp, 0);

	bRes = pC->EndReadBlob();
	TEST_CHECK(bRes);

	// this should fail since we're out of blobs to read
	bRes = pC->StartReadBlob(uID);
	TEST_CHECK(!bRes);

	// close should succeed
	bRes = pC->Close();
	TEST_CHECK(bRes);

	pCSPtr.reset();
	pStream.reset();
}

TEST_CASE(container_md5)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

//	_unlink("container_test.bin");
//	blocking_sharedptr pStream = MpoFileStream::GetInstance("container_test.bin", MPO_OPEN_READWRITE);
	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	const char pBuf[] = "ABC";
	char dstbuf[80];
	bool bRes;
	size_t stRes;
	unsigned int id = 0x77;

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	bRes = pC->StartWriteBlob(id);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(pBuf, sizeof(pBuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf));

	// now corrupt the last byte of the data we wrote
	TEST_REQUIRE(pStream->Seek(-1, MPO_SEEK_CUR));
	stRes = pStream->Write(pBuf, 1);	// write incorrect byte
	TEST_REQUIRE_EQUAL(stRes, 1);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	// re-open without closing the previous pCSPtr explicitly! (this actually revealed a bug)
	pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	pC = pCSPtr.get();

	bRes = pC->ReadHeader();
	TEST_REQUIRE(bRes);

	bRes = pC->StartReadBlob(id);
	TEST_REQUIRE(bRes);

	stRes = pC->ReadFromBlob(dstbuf, sizeof(dstbuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf));

	// MD5 checksum should've failed here
	bRes = pC->EndReadBlob();
	TEST_REQUIRE(!bRes);

	// NOW RE-OPEN AND TURN OFF MD5 CHECKING
	pCSPtr = MpoContainerFactory::CreateInstance(pStream, true);
	pC = pCSPtr.get();

	bRes = pC->ReadHeader();
	TEST_REQUIRE(bRes);

	bRes = pC->StartReadBlob(id);
	TEST_REQUIRE(bRes);

	stRes = pC->ReadFromBlob(dstbuf, sizeof(dstbuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf));

	// should return true because we have turned off MD5 checking
	bRes = pC->EndReadBlob();
	TEST_REQUIRE(bRes);

	// cleanup
	pCSPtr.reset();
	pStreamSPtr.reset();
}

TEST_CASE(container_jump)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

//	_unlink("container_test.bin");
//	blocking_sharedptr pStream = MpoFileStream::GetInstance("container_test.bin", MPO_OPEN_READWRITE);
	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	const char pBuf[] = "ABC";
	const char pBuf2[] = "DEFG";
	char dstbuf[80];
	bool bRes;
	size_t stRes;
	unsigned int id = 0x99;
	MPO_UINT64 u64 = 0;

	// jumping should fail since we haven't read or written a header
	bRes = pC->JumpToBlob(0);
	TEST_CHECK(!bRes);

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	// jumping should fail since we are in write mode
	bRes = pC->JumpToBlob(0);
	TEST_CHECK(!bRes);

	bRes = pC->StartWriteBlob(id);
	TEST_REQUIRE(bRes);

	// jumping should fail since we are in write mode
	bRes = pC->JumpToBlob(0);
	TEST_CHECK(!bRes);

	stRes = pC->WriteToBlob(pBuf, sizeof(pBuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf));

	// jumping should fail since we are in write mode
	bRes = pC->JumpToBlob(0);
	TEST_CHECK(!bRes);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	// jumping should fail since we are in write mode
	bRes = pC->JumpToBlob(0);
	TEST_CHECK(!bRes);

	// write blob #1
	bRes = pC->StartWriteBlob(id + 1);
	TEST_REQUIRE(bRes);
	stRes = pC->WriteToBlob(pBuf2, sizeof(pBuf2));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf2));
	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	// re-open without closing the previous pCSPtr explicitly! (this actually revealed a bug)
	pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	pC = pCSPtr.get();

	// jumping should fail since we haven't read the header yet
	bRes = pC->JumpToBlob(1);
	TEST_CHECK(!bRes);

	bRes = pC->ReadHeader();
	TEST_REQUIRE(bRes);

	// jumping should fail because the requested blob is out of range
	bRes = pC->JumpToBlob(2);
	TEST_REQUIRE(!bRes);

	// jumping should succeed since we are ready to call StartReadBlob and are in range
	bRes = pC->JumpToBlob(1);
	TEST_REQUIRE(bRes);

	u64 = pC->GetBlobIdx();
	TEST_CHECK_EQUAL(u64, 1);

	unsigned int uID = 0;
	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(bRes);
	TEST_REQUIRE_EQUAL(uID, id + 1);	// make sure ID is correct

	stRes = pC->ReadFromBlob(dstbuf, sizeof(dstbuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf2));

	bRes = pC->EndReadBlob();
	TEST_REQUIRE(bRes);

	// make sure memory matches up (I guess our MD5 check kinda already did this for us)
	int iMemCmp = memcmp(dstbuf, pBuf2, sizeof(pBuf2));
	TEST_CHECK_EQUAL(iMemCmp, 0);

	bRes = pC->JumpToBlob(0);
	TEST_REQUIRE(bRes);

	u64 = pC->GetBlobIdx();
	TEST_CHECK_EQUAL(u64, 0);

	bRes = pC->StartReadBlob(uID);
	TEST_REQUIRE(bRes);
	TEST_REQUIRE_EQUAL(uID, id);
	stRes = pC->ReadFromBlob(dstbuf, sizeof(dstbuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf));

	bRes = pC->EndReadBlob();
	TEST_REQUIRE(bRes);

	iMemCmp = memcmp(dstbuf, pBuf, sizeof(pBuf));
	TEST_CHECK_EQUAL(iMemCmp, 0);

	// cleanup
	pCSPtr.reset();
	pStreamSPtr.reset();
}

TEST_CASE(container_all_md5)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	const char pBuf[] = "ASDF";
	bool bRes;
	size_t stRes;

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	bRes = pC->StartWriteBlob(44732);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(pBuf, sizeof(pBuf));
	TEST_REQUIRE_EQUAL(stRes, sizeof(pBuf));

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	pCSPtr.reset();
	pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	pC = pCSPtr.get();

	// container verififcation should fail because we have not read the header yet
	bRes = pC->VerifyContainer();
	TEST_REQUIRE(!bRes);

	bRes = pC->ReadHeader();
	TEST_REQUIRE(bRes);

	// container verififcation should pass
	bRes = pC->VerifyContainer();
	TEST_REQUIRE(bRes);

	pCSPtr.reset();

	// change 1 byte to make sure verification fails
	pStream->Seek(30, MPO_SEEK_SET);	// choose arbitrary location that is within the md5 coverage area
	pStream->Write(pBuf, 1);

	pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	pC = pCSPtr.get();

	bRes = pC->ReadHeader();
	TEST_REQUIRE(bRes);

	// container verififcation should fail because the data is corrupt
	bRes = pC->VerifyContainer();
	TEST_REQUIRE(!bRes);

	// cleanup
	pCSPtr.reset();
	pStreamSPtr.reset();
}

// helper function
MPO_UINT64 get_max_blob_size(IBlockingStream *pStream)
{
	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();

	bool bRes = pC->ReadHeader();
	assert(bRes);

	return pC->GetMaxBlobSizeBytes();
}

TEST_CASE(container_max_blob_size1)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	unsigned char buf[80];	// arbitrary data
	bool bRes = false;
	size_t stRes = 0;
	MPO_UINT64 u64 = 0;

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	bRes = pC->StartWriteBlob(0);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 1);
	TEST_REQUIRE_EQUAL(stRes, 1);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	bRes = pC->StartWriteBlob(3);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 3);
	TEST_REQUIRE_EQUAL(stRes, 3);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	bRes = pC->StartWriteBlob(7);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 7);
	TEST_REQUIRE_EQUAL(stRes, 7);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	// now read it back
	pCSPtr.reset();

	u64 = get_max_blob_size(pStream);
	TEST_CHECK_EQUAL(u64, 7);

	// cleanup
	pStreamSPtr.reset();
}

TEST_CASE(container_max_blob_size2)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	unsigned char buf[80];	// arbitrary data
	bool bRes = false;
	size_t stRes = 0;
	MPO_UINT64 u64 = 0;

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	bRes = pC->StartWriteBlob(7);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 7);
	TEST_REQUIRE_EQUAL(stRes, 7);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	bRes = pC->StartWriteBlob(3);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 3);
	TEST_REQUIRE_EQUAL(stRes, 3);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	bRes = pC->StartWriteBlob(1);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 1);
	TEST_REQUIRE_EQUAL(stRes, 1);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	// now read it back
	pCSPtr.reset();

	u64 = get_max_blob_size(pStream);
	TEST_CHECK_EQUAL(u64, 7);

	// cleanup
	pStreamSPtr.reset();
}

TEST_CASE(container_max_blob_size3)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	unsigned char buf[80];	// arbitrary data
	bool bRes = false;
	size_t stRes = 0;
	MPO_UINT64 u64 = 0;

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	bRes = pC->StartWriteBlob(0);
	TEST_REQUIRE(bRes);

	stRes = pC->WriteToBlob(buf, 1);
	TEST_REQUIRE_EQUAL(stRes, 1);

	bRes = pC->EndWriteBlob();
	TEST_REQUIRE(bRes);

	// now read it back
	pCSPtr.reset();

	u64 = get_max_blob_size(pStream);
	TEST_CHECK_EQUAL(u64, 1);

	// cleanup
	pStreamSPtr.reset();
}

TEST_CASE(container_max_blob_size4)
{
	blocking_sharedptr pStreamSPtr = MpoMemStreamFactory::CreateInstance();
	IBlockingStream *pStream = pStreamSPtr.get();

	IMpoContainerSPtr pCSPtr = MpoContainerFactory::CreateInstance(pStream);
	IMpoContainer *pC = pCSPtr.get();
	bool bRes = false;
	MPO_UINT64 u64 = 0;

	bRes = pC->WriteHeader();
	TEST_CHECK(bRes);

	// NO BLOBS (0)

	// now read it back
	pCSPtr.reset();

	u64 = get_max_blob_size(pStream);
	TEST_CHECK_EQUAL(u64, 0);

	// cleanup
	pStreamSPtr.reset();
}
