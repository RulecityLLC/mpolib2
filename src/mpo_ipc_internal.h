//
// Created by Matt on 10/7/2019.
//

#ifndef MPO2_MPO_IPC_INTERNAL_H
#define MPO2_MPO_IPC_INTERNAL_H

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif // WIN32

#include <mpolib/mpo_ipc.h>
#include <mpolib/mpo_deleter.h>
#include <mpolib/mpo_thread.h>

// if defined, we will use pipes in unix.  Otherwise we will use sockets.
// (I've been having problems with sockets on OSX so I'm trying pipes)
#define UNIX_USE_PIPES

class MpoPipe : public IMpoPipe, public MpoDeleter
{
public:

static bool Create(IMpoPipeSPtr &pipe1, IMpoPipeSPtr &pipe2);

bool BlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead);

#ifndef WIN32
// not available on windows, used by NonblockingRead for MpoPipeEx for unix
	bool WouldReadNotBlock();
#endif

bool BlockingWrite(const void *pBuf, size_t stBytesToWrite, size_t *pstBytesWritten);

// Forcefully closes the pipe.
// This is useful if one thread is blocking on a read and the other thread wants to close the pipe.
// (in this case, the shared pointer can't be destroyed yet)
// 'uMsToWait' is for unit testing only (to test mutex protection).  It should always be 0 for production use.
void Close(unsigned int uMsToWait = 0);

private:
	MpoPipe();
	~MpoPipe();

	void DeleteInstance() { delete this; }

bool m_bPipeOpen;

#ifdef WIN32
// WIN32
HANDLE m_hRead, m_hWrite;
#else
// UNIX
#ifdef UNIX_USE_PIPES
	int m_pipeRead, m_pipeWrite;
#else
	int m_socket;
#endif // PIPES
#endif

// To make MpoPipe thread safe. (two threads may share the same shared pointers and both may try to call any of the public functions at the same time)
// This happened with MpoProcess shutting down one of its listener threads AND shutting down itself simultaneously.
MpoThreadMutexSPtr m_mutex;
MpoThreadMutex *m_pMutex;
};

class MpoPipeEx : public IMpoPipeEx, public MpoDeleter
{
public:
static bool Create(IMpoPipeExSPtr &pipe1, IMpoPipeExSPtr &pipe2);

// so that we can tell whether we would block if we called BlockingRead
size_t GetReadBufferSize();

bool BlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead);

bool BlockingWrite(const void *pBuf, size_t stBytesToWrite, size_t *pstBytesWritten);

bool NonBlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead);

void Close();

private:
	MpoPipeEx();
	~MpoPipeEx();

	void DeleteInstance() { delete this; }

bool BlockingReadInternal(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead);

IMpoPipeSPtr m_pipeSPtr;
IMpoPipe *m_pPipe;

MpoThreadMutexSPtr m_mutexWSPtr, m_mutexRSPtr;
MpoThreadMutex *m_pMutexW, *m_pMutexR;

// How many bytes are currently in the buffer (so that we know whether a call to BlockingRead would block or not).

// NOTE : these variables must be signed because of the following situation:
// - blocking read is called, it grabs the mutex
// - blocking write is called
// - blocking write attempts to update the buffer size, but the mutex is grabbed, so it blocks
// - blocking read wakes up because the pipe has data in it
// - blocking read updates the buffer size which is now a negative number
// - blocking read releases the mutex
// - blocking write wakes up, updates the buffer size through addition, so now the buffer size is correctly 0

// Blocking write MUST not grab the mutex before it writes because this could lead to a dead lock situation, if blocking read has already been called.

// (These variables must be protected by a mutex because their data lives in multiple classes and may be accessed by multiple threads.)
// If the class is writing, it refers to m_stBufferSizeW which lives within itself.
int m_iBufferSizeW;
// If the class is reading, it refers to m_pstBufferSizeR which lives in its sibling class.
int *m_piBufferSizeR;

// This variable MUST only be modified by the BlockingWrite function.  The NonblockingRead function may use this to determine if a read would block.
// This variable is not protected by a mutex because the BlockingWrite function isn't allowed to grab the mutex when this variable needs to be modified (read comments in that function as to why).
// This variable is how many bytes the BlockingWrite function is trying to write.
size_t m_stPendingBytesToBeWrittenW;
const size_t *m_pstPendingBytesToBeWrittenR;
};

#endif //MPO2_MPO_IPC_INTERNAL_H
