#ifndef MPO_FILE_STREAM_H
#define MPO_FILE_STREAM_H

#include "mpo_stream.h"

class IMpoFileStream : public IBlockingStream
{
public:
	virtual void Truncate(MPO_UINT64 u64FileSize) = 0;
};

typedef shared_ptr<IMpoFileStream> IMpoFileStreamSPtr;

class EXPORT_ME MpoFileStreamFactory
{
public:

	// for backward compatibility with legacy blocking_sharedptr; CreateInstance throws exception on error, GetInstance does not.
	static blocking_sharedptr CreateInstance(const char *filename, open_type flags);
	static blocking_sharedptr GetInstance(const char *filename, open_type flags);
	static blocking_sharedptr CreateInstance(const wstring &filename, open_type flags);
	static blocking_sharedptr GetInstance(const wstring &filename, open_type flags);

	// if you need to also be able to Truncate the file
	static IMpoFileStreamSPtr CreateInstanceFileStream(const char *filename, open_type flags);
	static IMpoFileStreamSPtr CreateInstanceFileStream(const wstring &filename, open_type flags);
};

/////////////////////////////////////////////////////////////////

#endif // MPO_FILE_STREAM_H
