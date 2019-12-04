#ifndef MPO_MEMORY_STREAM_H
#define MPO_MEMORY_STREAM_H

#include "mpo_stream.h"
#include <string>

using namespace std;

class EXPORT_ME MpoMemStreamFactory
{
public:
	// returns an instance with an empty stream
	static blocking_sharedptr CreateInstance();

	static blocking_sharedptr CreateInstance(void *pBuf, size_t stBufLength);
};

#endif // MPO_MEMORY_STREAM_H
