#ifndef MPO_FILE_STREAM_H
#define MPO_FILE_STREAM_H

#include "mpo_stream.h"

class EXPORT_ME MpoFileStreamFactory
{
public:
	static blocking_sharedptr CreateInstance(const char *filename, open_type flags);

	static blocking_sharedptr CreateInstance(const wstring &filename, open_type flags);
};

/////////////////////////////////////////////////////////////////

#endif // MPO_FILE_STREAM_H
