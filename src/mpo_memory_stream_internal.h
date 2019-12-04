//
// Created by Matt on 10/7/2019.
//

#ifndef MPO2_MPO_MEMORY_STREAM_INTERNAL_H
#define MPO2_MPO_MEMORY_STREAM_INTERNAL_H

#include <mpolib/mpo_stream.h>
#include <string>

using namespace std;

class MpoMemStream : public IBlockingStream, public MpoDeleter
{
public:
// returns an instance with an empty stream
static blocking_sharedptr CreateInstance();

// returns an instance, using a buffer to initially populated the stream
static blocking_sharedptr CreateInstance(void *pBuf, size_t stBufLength);

size_t Read(void *buf, size_t stBytesToRead);

size_t Write(const void *buf, size_t stBytesToWrite);

bool Seek(MPO_INT64 i64Offset, seek_type origin);

// returns length of stream
MPO_UINT64 GetLength();

// returns current position within stream
MPO_UINT64 GetPosition();

bool CanRead();

bool CanWrite();

bool CanSeek();
private:
	MpoMemStream();

	virtual ~MpoMemStream();

	void DeleteInstance() { delete this; }

MPO_UINT64 m_u64Pos;

MPO_UINT64 m_u64TotalBytes;

// this serves as a growable buffer behind-the-scenes
string m_strBuf;
};

#endif //MPO2_MPO_MEMORY_STREAM_INTERNAL_H
