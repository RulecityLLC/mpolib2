#include <string.h>
#include <mpolib/mpo_memory_stream.h>
#include "mpo_memory_stream_internal.h"

blocking_sharedptr MpoMemStreamFactory::CreateInstance()
{
	return MpoMemStream::CreateInstance();
}

blocking_sharedptr MpoMemStream::CreateInstance()
{
	blocking_sharedptr pRes;

	MpoMemStream *pInstance = new MpoMemStream();
	pInstance->m_u64Pos = 0;
	pInstance->m_u64TotalBytes = 0;
	pInstance->m_strBuf.erase();
	pRes = blocking_sharedptr(pInstance, MpoMemStream::deleter());

	return pRes;
}

blocking_sharedptr MpoMemStreamFactory::CreateInstance(void *pBuf, size_t stBufLength)
{
	return MpoMemStream::CreateInstance(pBuf, stBufLength);
}

blocking_sharedptr MpoMemStream::CreateInstance(void *pBuf, size_t stBufLength)
{
	blocking_sharedptr pRes;
	blocking_sharedptr pTmp = CreateInstance();
	if (pTmp.get())
	{
		if (pTmp->Write(pBuf, stBufLength) == stBufLength)
		{
			if (pTmp->Seek(0, MPO_SEEK_SET))
			{
				// if everything's gone right
				pRes = pTmp;
			}
		}
	}
	return pRes;
}

blocking_sharedptr MpoMemStreamFactory::GetInstance()
{
	return CreateInstance();
}

blocking_sharedptr MpoMemStreamFactory::GetInstance(void* pBuf, size_t stBufLength)
{
	return CreateInstance(pBuf, stBufLength);
}

size_t MpoMemStream::Read(void *buf, size_t stBytesToRead)
{
	size_t stRes = 0;
	const unsigned char *u8Buf = (const unsigned char *) m_strBuf.data();
	MPO_UINT64 u64BytesLeft = 0;
	
	// if our current position isn't passed the end of the stream
	if (m_u64TotalBytes >= m_u64Pos)
	{
		u64BytesLeft = m_u64TotalBytes - m_u64Pos;
	}

	// if we're trying to read too far
	if (stBytesToRead > u64BytesLeft)
	{
		stBytesToRead = (size_t) u64BytesLeft;
	}

	// if we actually have some bytes to read
	if (stBytesToRead > 0)
	{
		stRes = stBytesToRead;
		memcpy(buf, u8Buf + m_u64Pos, stBytesToRead);
		m_u64Pos += stBytesToRead;
	}

	return stRes;
}

size_t MpoMemStream::Write(const void *buf, size_t stBytesToWrite)
{
	// if we're overwriting part of the stream
	if (m_strBuf.size() > m_u64Pos)
	{
		// erase what we're overwriting
		m_strBuf.erase((size_t) m_u64Pos, stBytesToWrite);
	}
	// else if we need to pad the stream
	else if (m_u64Pos > m_strBuf.size())
	{
		size_t stPadding = (size_t) m_u64Pos - m_strBuf.size();
		m_strBuf.append(stPadding, (char) 0);	// we pad 0's, which are as good as anything
	}

	// insert what we've written
	m_strBuf.insert((size_t) m_u64Pos, string((char *) buf, stBytesToWrite));

	// advance position according to how many bytes we've written
	m_u64Pos += stBytesToWrite;

	// update total byte count
	m_u64TotalBytes = m_strBuf.size();

	return stBytesToWrite;
}

bool MpoMemStream::Seek(MPO_INT64 i64Offset, seek_type origin)
{
	bool bRes = false;

	// negated offset (used for safety checking if the offset is negative)
	MPO_INT64 i64NegOffset = i64Offset * -1;

	// if the offset is negative
	bool bNeg = false;

	if (i64Offset < 0)
	{
		bNeg = true;
	}

	switch (origin)
	{
	case MPO_SEEK_END:
		if (!bNeg || ((MPO_UINT64) i64NegOffset <= m_u64TotalBytes))
		{
			m_u64Pos = m_u64TotalBytes + i64Offset;
			bRes = true;
		}
		// else we're trying to go past the beginning of the file
		break;
	case MPO_SEEK_SET:
		// no negative offset when starting at the beginning of the file
		if (!bNeg)
		{
			m_u64Pos = i64Offset;
			bRes = true;
		}
		break;
	// SEEK CUR
	default:
		if (!bNeg || ((MPO_UINT64) i64NegOffset <= m_u64Pos))
		{
			m_u64Pos = m_u64Pos + i64Offset;
			bRes = true;
		}
		// else we're trying to seek past the beginning of the file which isn't allowed
		break;
	}

	return bRes;
}

MPO_UINT64 MpoMemStream::GetLength()
{
	return m_u64TotalBytes;
}

MPO_UINT64 MpoMemStream::GetPosition()
{
	return m_u64Pos;
}

bool MpoMemStream::CanRead()
{
	return true;
}

bool MpoMemStream::CanWrite()
{
	return true;
}

bool MpoMemStream::CanSeek()
{
	return true;
}

MpoMemStream::MpoMemStream() :
m_u64Pos(0),
m_u64TotalBytes(0)
{
}

MpoMemStream::~MpoMemStream()
{
}
