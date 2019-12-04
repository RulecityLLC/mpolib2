#ifndef MPO_IPC_H
#define MPO_IPC_H

#include "mpo_dll.h"
#include "mpo_deleter.h"
#include <stdio.h>	// for size_t

class IMpoPipe {
public:

	virtual bool BlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead) = 0;

	virtual bool BlockingWrite(const void *pBuf, size_t stBytesToWrite, size_t *pstBytesWritten) = 0;

// Forcefully closes the pipe.
// This is useful if one thread is blocking on a read and the other thread wants to close the pipe.
// (in this case, the shared pointer can't be destroyed yet)
// 'uMsToWait' is for unit testing only (to test mutex protection).  It should always be 0 for production use.
	virtual void Close(unsigned int uMsToWait = 0) = 0;
};

typedef shared_ptr<IMpoPipe> IMpoPipeSPtr;

class EXPORT_ME MpoPipeFactory
{
public:
	static void Create(IMpoPipeSPtr &pipe1, IMpoPipeSPtr &pipe2);
};

// The purpose of this interface is to make it easy to mock out this MpoPipeEx in a unit test
class IMpoPipeEx
{
public:
	virtual size_t GetReadBufferSize() = 0;

	virtual bool BlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead) = 0;

	virtual bool BlockingWrite(const void *pBuf, size_t stBytesToWrite, size_t *pstBytesWritten) = 0;

	// Same as BlockingRead but will immediately return with 0 bytes read if no bytes are pending.
	// WARNING: this function _will_ block if BlockingRead is called by another thread and blocks.
	// The idea is that only one thread should be calling the read functions in this class.
	virtual bool NonBlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead) = 0;

	// Forcefully closes the pipe.
	// This is useful if one thread is blocking on a read and the other thread wants to close the pipe.
	// (in this case, the shared pointer can't be destroyed yet)
	virtual void Close() = 0;
};

typedef shared_ptr<IMpoPipeEx> IMpoPipeExSPtr;

class EXPORT_ME MpoPipeExFactory
{
public:
	static void Create(IMpoPipeExSPtr &pipe1, IMpoPipeExSPtr &pipe2);
};

#endif // MPO_IPC_H
