#include <string.h>
#include "mpo_container_internal.h"
#include <mpolib2/mpo_misc.h>

// spells "1CON" in ASCII in little-endian format, last digit can be incremented to increase version
const unsigned int CONTAINER_VERSION = 0x4E4F4331;

// ALL INTEGERS ARE LITTLE ENDIAN UNLESS OTHERWISE SPECIFIED
// Container Header description:
//  4 bytes: version
// 16 bytes: MD5 of all bytes that occur _after_ this point (so we can verify almost everything in the container)
//  8 bytes: total number of blobs
//  8 bytes: offset to table of contents

// Blob header description:
// Blob's size in bytes (8 bytes)
// MD5 of this blob's data (16 bytes)
// Reserved (must be 0) (4 bytes)
// Arbitrary Chunk ID (4 bytes)

IMpoContainerSPtr MpoContainerFactory::CreateInstance(IBlockingStream *pStream, bool bIgnoreReadIntegrity)
{
    IMpoContainerSPtr pRes;

	// stream must be seekable
	if (pStream->CanSeek())
	{
		MpoContainer *pInstance = new MpoContainer();
		if (pInstance)
		{
			// Don't seek here because if caller overwrites an open MpoContainer instance, using the stream,
			//  the old instance's destructor will occur after this constructor.
			// So we should seek in the [Read/Write]Header functions instead.
			pInstance->m_pStream = pStream;
			pInstance->m_bIgnoreReadIntegrity = bIgnoreReadIntegrity;
			pRes = IMpoContainerSPtr(pInstance, MpoContainer::deleter());
		}
	}

	return pRes;
}

bool MpoContainer::JumpToBlob(MPO_UINT64 uBlobIdx)
{
	bool bRes = false;

	// can only jump when reading (by design) and when we aren't in the middle of a read
	if ((!m_bReadHeader) || (m_bReadBlobStarted))
	{
		return false;
	}

	// range check
	if (uBlobIdx >= m_Header.u64TotalBlobs)
	{
		return false;
	}

	// seek to beginning of blob
	// WARNING : since I cast uBlobIdx to a 32-bit value, this means that we cannot support more than 2^32 blobs, although the file specs allow for it.
	// If a need for a large number of blobs arises, this vector scheme will need to be rewritten.
	m_pStream->Seek(m_vTOC[(size_t) uBlobIdx], MPO_SEEK_SET);
	m_u64CurBlobIdx = uBlobIdx;
	bRes = true;

	return bRes;
}

bool MpoContainer::ReadHeader()
{
	bool bRes = false;
	unsigned char buf[16];

	// can't read after we've written
	if ((m_bWroteHeader) || (m_bReadHeader))
	{
		return false;
	}

	m_pStream->Seek(0, MPO_SEEK_SET);	// rewind in case caller didn't do this for us

	// get version
	if (m_pStream->Read(buf, 4) != 4)
	{
		return false;
	}
	m_Header.uVersion = mpom::read_lile32(buf);

	// version check
	if (m_Header.uVersion != CONTAINER_VERSION)
	{
		return false;
	}

	// now read total MD5
	if (m_pStream->Read(buf, 16) != 16)
	{
		return false;
	}
	memcpy(m_Header.TotalMD5, buf, 16);

	// now read total chunks
	if (m_pStream->Read(buf, 8) != 8)
	{
		return false;
	}
	m_Header.u64TotalBlobs = mpom::read_lile64(buf);

	// now read TOC offset
	if (m_pStream->Read(buf, 8) != 8)
	{
		return false;
	}
	m_Header.u64OffsetTOC = mpom::read_lile64(buf);

	MPO_UINT64 u64CurPos = m_pStream->GetPosition();	// so we can return here later

	m_pStream->Seek(m_Header.u64OffsetTOC, MPO_SEEK_SET);	// seek to TOC and read it in full

	// read every TOC entry
	for (MPO_UINT64 u64BlobIdx = 0; u64BlobIdx < m_Header.u64TotalBlobs; u64BlobIdx++)
	{
		m_pStream->Read(buf, 8);
		MPO_UINT64 u64Offset = mpom::read_lile64(buf);
		m_vTOC.push_back(u64Offset);
	}

	// begin calculate max blob size
	MPO_UINT64 u64Diff = 0;
	for (size_t stIdx = 1; stIdx < m_vTOC.size(); stIdx++)
	{
		u64Diff = m_vTOC[stIdx] - m_vTOC[stIdx-1];	// this will always be valid because our idx starts at 1, not 0
		u64Diff -= 32;	// blob's header is 32 bytes big

		// if we have a new maximum
		if (u64Diff > m_u64MaxBlobSizeBytes)
		{
			m_u64MaxBlobSizeBytes = u64Diff;
		}
	}

	// do we have at least 1 blob?
	if (!m_vTOC.empty())
	{
		// get the size of the last blob (if it exists)
		u64Diff = m_vTOC[m_vTOC.size() - 1];

		u64Diff = m_Header.u64OffsetTOC - u64Diff;
		u64Diff -= 32;	// blob's header is 32 bytes big

		// if we have a new maximum
		if (u64Diff > m_u64MaxBlobSizeBytes)
		{
			m_u64MaxBlobSizeBytes = u64Diff;
		}
	}
	// end calculate max blob size
	
	m_pStream->Seek(u64CurPos, MPO_SEEK_SET);	// return to where we were

	m_bReadHeader = true;
	bRes = true;

	return bRes;
}

bool MpoContainer::StartReadBlob(unsigned int &uID)
{
	bool bRes = false;

	// we can't start reading a new blob until the header has been read and any previous blobs we've been reading are closed
	if ((!m_bReadHeader) || (m_bReadBlobStarted))
	{
		return false;
	}

	// check to make sure we aren't out of range
	if (m_u64CurBlobIdx >= m_Header.u64TotalBlobs)
	{
		return false;
	}

	m_u64CurBlobStartOffset = m_pStream->GetPosition();

	// Blob header is defined as such:
	// Blob's size in bytes (8 bytes)
	// MD5 of this blob (16 bytes)
	// Chunk ID (4 bytes)
	unsigned char buf[16];
	if (!m_pStream->Read(buf, 8)) return false;	// byte count
	m_u64BlobSizeBytes = mpom::read_lile64(buf);
	if (!m_pStream->Read(m_arrBlobMD5, 16)) return false;	// blob's MD5
	if (!m_pStream->Read(buf, 4)) return false;	// reserved
	uID = mpom::read_lile32(buf);	// make sure all bytes are 0, to ensure bad containers aren't created
	if (uID != 0) return false;
	if (!m_pStream->Read(buf, 4)) return false;	// chunk ID
	uID = mpom::read_lile32(buf);

	if (!m_bIgnoreReadIntegrity)
	{
		MD5Init(&m_md5CurBlob);	// get ready to verify blob integrity
	}

	m_u64CurBytesRead = 0;
	m_bReadBlobStarted = true;
	bRes = true;

	return bRes;
}

MPO_UINT64 MpoContainer::GetCurBlobSizebytes()
{
	return m_u64BlobSizeBytes;
}

size_t MpoContainer::ReadFromBlob(void *buf, size_t stNumBytes)
{
	size_t stRes = 0;

	if (m_bReadBlobStarted)
	{
		// check to make sure they can't read passed the end
		if ((stNumBytes + m_u64CurBytesRead) > m_u64BlobSizeBytes)
		{
			stNumBytes = (size_t) (m_u64BlobSizeBytes - m_u64CurBytesRead);
		}
		stRes = m_pStream->Read(buf, stNumBytes);
		m_u64CurBytesRead += stRes;

		if (!m_bIgnoreReadIntegrity)
		{
			MD5Update(&m_md5CurBlob, (const unsigned char*) buf, stRes);
		}
	}

	return stRes;
}

// will return false if blob was corrupt (MD5 check failed)
bool MpoContainer::EndReadBlob()
{
	bool bRes = false;

	// if blob has been opened properly
	if (m_bReadBlobStarted)
	{
		// if blob has been completely read
		if (m_u64CurBytesRead == m_u64BlobSizeBytes)
		{
			m_bReadBlobStarted = false;
			m_u64CurBlobIdx++;	// move on to the next blob

			if (!m_bIgnoreReadIntegrity)
			{
				unsigned char md5[16];
				MD5Final(md5, &m_md5CurBlob);

				// if MD5 matches
				if (memcmp(md5, m_arrBlobMD5, 16)==0)
				{
					bRes = true;
				}
				// else MD5 failed
			}
			// else we aren't checking MD5, so this function always returns true
			else
			{
				bRes = true;
			}
		}
	}
	return bRes;
}

bool MpoContainer::WriteHeader()
{
	bool bRes = false;
	unsigned char buf[16];	// temporary buffer
	
	if ((m_bWroteHeader) || (m_bReadHeader))
	{
		return false;
	}

	m_pStream->Seek(0, MPO_SEEK_SET);	// rewind in case caller didn't do this for us

	if (!m_pStream->Write(&CONTAINER_VERSION, 4)) return false;	// version
	if (!m_pStream->Write(buf, 16)) return false;	// placeholder for total MD5
	if (!m_pStream->Write(buf, 8)) return false;	// placeholder for total chunks
	if (!m_pStream->Write(buf, 8)) return false;	// placeholder for TOC offset
	m_bWroteHeader = bRes = true;
	m_bHeaderNeedsUpdate = true;	// when container is closed, the placeholder stuff in the header will need to be filled in

	return bRes;
}

bool MpoContainer::StartWriteBlob(unsigned int id)
{
	bool bRes = false;

	if ((m_bWroteHeader) && (!m_bWriteBlobStarted))
	{
		// needed when we close the blob
		m_u64CurBlobStartOffset = m_pStream->GetPosition();

		// Blob header is defined as such:
		// Blob's size in bytes (8 bytes)
		// MD5 of this blob (16 bytes)
		// Chunk ID (4 bytes)
		unsigned char buf[16];
		if (!m_pStream->Write(buf, 8)) return false;	// placeholder for byte count
		if (!m_pStream->Write(buf, 16)) return false;	// placeholder for blob's MD5
		memset(buf, 0, 4);
		if (!m_pStream->Write(buf, 4)) return false;	// reserved
		mpom::write_lile32(buf, id);
		if (!m_pStream->Write(buf, 4)) return false;	// chunk ID

		m_u64CurBytesWritten = 0;
		m_bWriteBlobStarted = true;

		// reset MD5 context
		MD5Init(&m_md5CurBlob);

		bRes = true;
	}
	// else header has not been written

	return bRes;
}

size_t MpoContainer::WriteToBlob(const void *buf, size_t stNumBytes)
{
	size_t stRes = 0;

	if (m_bWriteBlobStarted)
	{
		stRes = m_pStream->Write(buf, stNumBytes);
		m_u64CurBytesWritten += stRes;
		MD5Update(&m_md5CurBlob, (const unsigned char *) buf, stNumBytes);
	}

	return stRes;
}

bool MpoContainer::EndWriteBlob()
{
	bool bRes = false;

	if (m_bWriteBlobStarted)
	{
		unsigned char buf[16];

		// seek back to blob header start
		if (!m_pStream->Seek(m_u64CurBlobStartOffset, MPO_SEEK_SET)) return false;
		mpom::write_lile64(buf, m_u64CurBytesWritten);	// byte count
		if (!m_pStream->Write(buf, 8)) return false;

		MD5Final(buf, &m_md5CurBlob);
		if (!m_pStream->Write(buf, 16)) return false;

		// 4 reserved bytes and 4 bytes of ID are already written
		if (!m_pStream->Seek(m_u64CurBytesWritten + 8, MPO_SEEK_CUR)) return false;	// seek back to where we came from
		m_Header.u64TotalBlobs++;
		m_u64CurBlobIdx++;	// this will actually be the same number as the total blobs

		// add offset to table of contents
		m_vTOC.push_back(m_u64CurBlobStartOffset);

		m_bWriteBlobStarted = false;
		bRes = true;
	}

	return bRes;
}

bool MpoContainer::Close(CallbackProc pCallback, size_t stUpdateIntervalBytes)
{
	bool bRes = false;

	// if we need to close a previous write
	if (m_bWriteBlobStarted)
	{
		EndWriteBlob();
	}

	if (m_bHeaderNeedsUpdate)
	{
		unsigned char buf[16];	// temp buf

		// get TOC offset; should be at our current file position
		m_Header.u64OffsetTOC = m_pStream->GetPosition();

		// write TOC
		for (vector<MPO_UINT64>::const_iterator li = m_vTOC.begin();
			li != m_vTOC.end();
			li++)
		{
			mpom::write_lile64(buf, *li);
			m_pStream->Write(buf, 8);
		}

		// TOC written, go to beginning header and write total chunks and TOC offset
		if (!m_pStream->Seek(20, MPO_SEEK_SET)) return false;

		// write total chunks
		mpom::write_lile64(buf, m_Header.u64TotalBlobs);
		if (!m_pStream->Write(buf, 8)) return false;

		// write TOC offset
		mpom::write_lile64(buf, m_Header.u64OffsetTOC);
		if (!m_pStream->Write(buf, 8)) return false;

		// now read entire container (unfortunately) and compute MD5
		if (!m_pStream->Seek(20, MPO_SEEK_SET)) return false;

		// TODO : allow callback
		MD5Init(&m_md5Container);
		const unsigned int BUFSIZE = 1024*1024;
		SHARED_ARRAY(unsigned char) pBufSPtr(new unsigned char[BUFSIZE]);
		unsigned char *pBuf = pBufSPtr.get();
		for (;;)
		{
			size_t stBytesRead = m_pStream->Read(pBuf, BUFSIZE);
			MD5Update(&m_md5Container, pBuf, stBytesRead);
			if (stBytesRead != BUFSIZE)
			{
				break;
			}
		}
		MD5Final(m_Header.TotalMD5, &m_md5Container);

		// write MD5 info
		if (!m_pStream->Seek(4, MPO_SEEK_SET)) return false;
		if (!m_pStream->Write(m_Header.TotalMD5, 16)) return false;

		bRes = true;
		m_bHeaderNeedsUpdate = false;
	}
	// else if header does not need update
	else
	{
		bRes = true;
	}

	return bRes;
}

MPO_UINT64 MpoContainer::GetBlobIdx() const
{
	return m_u64CurBlobIdx;
}

MPO_UINT64 MpoContainer::GetBlobCount() const
{
	return m_Header.u64TotalBlobs;
}

MPO_UINT64 MpoContainer::GetMaxBlobSizeBytes() const
{
	return m_u64MaxBlobSizeBytes;
}

bool MpoContainer::VerifyContainer(CallbackProc pCallback, size_t stUpdateIntervalBytes)
{
	bool bRes = false;

	if (!m_bReadHeader)
	{
		return false;
	}

	// this can only be called after the header is read (to keep things simple)
	if (m_u64CurBlobIdx != 0)
	{
		return false;
	}

	// they can't be in the middle of a read
	if (m_bReadBlobStarted)
	{
		return false;
	}

	m_pStream->Seek(0, MPO_SEEK_END);
	MPO_UINT64 u64TotalBytes = m_pStream->GetPosition() - 20;	// get the number of bytes we need to scan in order to have accurate results
	MPO_UINT64 u64FinishedBytes = 0;	// how many bytes we've read in thus far

	// seek to where MD5 data coverage begins
	m_pStream->Seek(20, MPO_SEEK_SET);

	size_t stBufSize = stUpdateIntervalBytes;

	// make sure our buffer makes sense (keep it within a reasonable range)
	if ((stBufSize == 0) || (stBufSize > (1<<22)))
	{
		stBufSize = (1<<22);
	}

    SHARED_ARRAY(unsigned char) pBufSPtr(new unsigned char [stBufSize]);    // to ensure buffer gets de-allocated properly
	unsigned char *pBuf = pBufSPtr.get();

	oMD5_CTX md5c;
	MD5Init(&md5c);
	for (;;)
	{
		size_t stBytesRead = m_pStream->Read(pBuf, stBufSize);
		u64FinishedBytes += stBytesRead;

		MD5Update(&md5c, (const unsigned char *) pBuf, stBytesRead);

		if (pCallback != 0)
		{
			bool bKeepGoing = pCallback(u64FinishedBytes, u64TotalBytes);
			if (!bKeepGoing) break;	// this will cause the md5 to be wrong, which will cause us to return false, which is the correct behavior we want
		}

		// if we've hit EOF
		if (stBytesRead != stBufSize)
		{
			break;
		}
	}
	unsigned char md5[16];
	MD5Final(md5, &md5c);

	if (memcmp(md5, m_Header.TotalMD5, 16)==0)
	{
		bRes = true;
	}

	return bRes;
}

/////////////////////////////////////////

MpoContainer::MpoContainer() :
m_bReadHeader(false),
m_bWroteHeader(false),
m_bHeaderNeedsUpdate(false),
m_bWriteBlobStarted(false),
m_u64CurBytesWritten(0),
m_u64CurBlobStartOffset(0),
m_u64BlobSizeBytes(0),
m_u64CurBytesRead(0),
m_bReadBlobStarted(false),
m_u64CurBlobIdx(0),
m_u64MaxBlobSizeBytes(0)
{
	// initialize to default values
	memset(&m_Header, 0, sizeof(m_Header));
	memset(m_Header.TotalMD5, 0xDD, 16);	// to make debugging easier
	m_Header.u64OffsetTOC = ~0;	// to make debugging easier
	memset(&m_arrBlobMD5, 0, sizeof(m_arrBlobMD5));
}

MpoContainer::~MpoContainer()
{
	Close();	// in case caller doesn't call this
}

///////////////

blocking_sharedptr MpoContainerStream::CreateInstance (IMpoContainer *pCon)
{
	MpoContainerStream *pStream = new MpoContainerStream();
	pStream->m_pCon = pCon;
	return blocking_sharedptr(pStream, MpoContainerStream::deleter());
}

size_t MpoContainerStream::Read(void *buf, size_t stBytesToRead)
{
	return 0;
}

size_t MpoContainerStream::Write(const void *buf, size_t stBytesToWrite)
{
	return m_pCon->WriteToBlob(buf, stBytesToWrite);
}

bool MpoContainerStream::Seek(MPO_INT64 i64Offset, seek_type origin)
{
	return false;
}

MPO_UINT64 MpoContainerStream::GetLength()
{
	return 0;
}

MPO_UINT64 MpoContainerStream::GetPosition()
{
	return 0;
}

bool MpoContainerStream::CanRead()
{
	return false;
}

bool MpoContainerStream::CanWrite()
{
	return true;
}

bool MpoContainerStream::CanSeek()
{
	return false;
}
