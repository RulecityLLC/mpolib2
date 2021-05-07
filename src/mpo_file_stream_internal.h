//
// Created by Matt on 10/4/2019.
//

#ifndef MPO2_MPO_FILE_STREAM_INTERNAL_H
#define MPO2_MPO_FILE_STREAM_INTERNAL_H

#include <mpolib/mpo_stream.h>
#include "mpo_fileio_internal.h"

class MpoFileStream : public IMpoFileStream, public MpoDeleter
{
public:
	static blocking_sharedptr GetInstance(const char *filename, open_type flags);

	static blocking_sharedptr GetInstance(const wstring &filename, open_type flags);

	static IMpoFileStreamSPtr GetInstanceFileStream(const char *filename, open_type flags);

	static IMpoFileStreamSPtr GetInstanceFileStream(const wstring &filename, open_type flags);

	size_t Read(void *buf, size_t stBytesToRead);

	size_t Write(const void *buf, size_t stBytesToWrite);

	bool Seek(MPO_INT64 i64Offset, seek_type origin);

	void Truncate(MPO_UINT64 u64FileSize) override;

	// returns length of stream
	MPO_UINT64 GetLength();

	// returns current position within stream
	MPO_UINT64 GetPosition();

	bool CanRead();

	bool CanWrite();

	bool CanSeek();

private:
	MpoFileStream();
	~MpoFileStream();

	void DeleteInstance() { delete this; }

	mpo_io *m_io;

};

#endif //MPO2_MPO_FILE_STREAM_INTERNAL_H
