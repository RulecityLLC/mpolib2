#include <mpolib/mpo_ipc.h>
#include <mpolib/mpo_timer.h>
#include <mpolib/mpo_net.h>	// for better_select (unix only)
#include <stdio.h>	// for debugging
#include <errno.h>	// for debugging
#include <string.h>	// for strerror_R
#include <assert.h>
#include "mpo_ipc_internal.h"
#include <stdexcept>

#ifndef WIN32
#include <sys/socket.h>
#include <sys/un.h>
#endif // not WIN32

#define UNIX_USE_PIPES

void MpoPipeFactory::Create(IMpoPipeSPtr &pipe0, IMpoPipeSPtr &pipe1)
{
	bool success = MpoPipe::Create(pipe0, pipe1);
	if (!success)
	{
		throw runtime_error("Pipe creation failed");
	}
}

bool MpoPipe::Create(IMpoPipeSPtr &pipe0, IMpoPipeSPtr &pipe1)
{
	bool bRes = false;
	MpoPipe *pInstance[2] = { NULL, NULL };

#ifdef WIN32
	HANDLE hReadPipe[2], hWritePipe[2];

	if (CreatePipe(&hReadPipe[0], &hWritePipe[0], NULL, 0) != 0)
	{
		if (CreatePipe(&hReadPipe[1], &hWritePipe[1], NULL, 0) != 0)
		{
			for (int i = 0; i < 2; i++)
			{
				pInstance[i] = new MpoPipe();
				pInstance[i]->m_hRead = hReadPipe[i];
				pInstance[i]->m_hWrite = hWritePipe[i ^ 1];	// make sure write pipe is opposite of read pipe
			}

			bRes = true;
		}
	}
#else // end WIN32

#ifdef UNIX_USE_PIPES
	int pipes0[2], pipes1[2];

	if (pipe(pipes0) == 0)
	{
//		printf("[%5u] Pipe created.  Read: %u, Write: %u\n", getpid(), pipes0[0], pipes0[1]);
		if (pipe(pipes1) == 0)
		{
//			printf("[%5u] Pipe created.  Read: %u, Write: %u\n", getpid(), pipes1[0], pipes1[1]);
			pInstance[0] = new MpoPipe();
			pInstance[0]->m_pipeRead = pipes0[0];
			pInstance[0]->m_pipeWrite = pipes1[1];	// make sure write pipe is opposite of read pipe
			pInstance[1] = new MpoPipe();
			pInstance[1]->m_pipeRead = pipes1[0];
			pInstance[1]->m_pipeWrite = pipes0[1];	// make sure write pipe is opposite of read pipe
			bRes = true;
		}
	}
#else
	int sv[2];
	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sv) == 0)
	{
//		printf("[%5u] Socketpair created sockets: %u and %u\n", getpid(), sv[0], sv[1]);
		for (int i = 0; i < 2; i++)
		{
			pInstance[i] = new MpoPipe();
			pInstance[i]->m_socket = sv[i];
		}
		bRes = true;
	}
#endif // end PIPES
#endif // end UNIX

	if (bRes)
	{
		pInstance[0]->m_bPipeOpen = pInstance[1]->m_bPipeOpen = true;
		pipe0 = IMpoPipeSPtr(pInstance[0], MpoPipe::deleter());
		pipe1 = IMpoPipeSPtr(pInstance[1], MpoPipe::deleter());
	}
	// else make sure that these pipes really point to NULL in case they are re-used from a previous session
	else
	{
		pipe0.reset();
		pipe1.reset();
	}

	return bRes;
}

bool MpoPipe::BlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead)
{
	bool bRes = false;

	// NOTE: we do not want a mutex around this code because it could block

#ifdef WIN32
	// WIN32
	BOOL b = ReadFile(m_hRead, pBuf, stBytesToRead, (LPDWORD) pstBytesRead, NULL);
	bRes = (b == TRUE);
#else
	// UNIX
//	int fd = 0;
	ssize_t sstRes = 0;

#ifdef UNIX_USE_PIPES
	/*fd = m_pipeRead;*/
	sstRes = read(m_pipeRead, pBuf, stBytesToRead);
#else
	/*fd = m_socket;*/
	sstRes = recv(m_socket, pBuf, stBytesToRead, 0);
#endif // PIPES
	if (sstRes > 0)
	{
		*pstBytesRead = sstRes;
		bRes = true;
	}
	else if (sstRes == 0)
	{
//		printf("[%5u] BlockingRead from %d EOF\n", getpid(), fd);
	}
	else
	{
#ifndef __APPLE__
//		char buf[80];
//		printf("BlockingRead read from %d failed: %s\n", fd, strerror_r(errno, buf, sizeof(buf)));
#else
//		printf("BlockingRead read from %d failed: %d\n", fd, errno);
#endif
	}
#endif

	return bRes;
}

#ifndef WIN32
// this call isn't possible on windows
bool MpoPipe::WouldReadNotBlock()
{
#ifdef UNIX_USE_PIPES
	int rv = better_select(m_pipeRead, SELECT_READ, 0);
#else
	int rv = better_select(m_socket, SELECT_READ, 0);
#endif // PIPES
	return (rv > 0);
}
#endif // WIN32

bool MpoPipe::BlockingWrite(const void *pBuf, size_t stBytesToWrite, size_t *pstBytesWritten)
{
	bool bRes = false;

	// NOTE: we do not want a mutex around this code because it could block

#ifdef WIN32
	BOOL b = WriteFile(m_hWrite, pBuf, stBytesToWrite, (LPDWORD) pstBytesWritten, 0);
	bRes = (b == TRUE);
#else

	// UNIX
//	int fd = 0;
	ssize_t sstRes = 0;

#ifdef UNIX_USE_PIPES
	//fd = m_pipeWrite;
	sstRes = write(m_pipeWrite, pBuf, stBytesToWrite);
#else
	//fd = m_socket;
	sstRes = send(m_socket, pBuf, stBytesToWrite, 0);
#endif // PIPES
	if (sstRes > 0)
	{
		*pstBytesWritten = sstRes;
		bRes = true;
	}
	else if (sstRes == 0)
	{
//		printf("BlockingWrite to %d EOF\n", fd);
	}
	else
	{
#ifndef __APPLE__
//		char buf[80];
//		printf("BlockingWrite write to %d failed: %s\n", fd, strerror_r(errno, buf, sizeof(buf)));
#else
//		printf("BlockingWrite write to %d failed: %d\n", fd, errno);
#endif
	}
#endif

	return bRes;
}

#ifdef WIN32
void HandleCloser(HANDLE h)
{
	BOOL b = CloseHandle(h);
	if (!b)
	{
		DWORD dwLastError = GetLastError();
#ifdef DEBUG
//		printf("CloseHandle failed, returned last error: %u\n", dwLastError);
#endif // DEBUG
	}
}
#endif // WIN32

void MpoPipe::Close(unsigned int uMsToWait)
{
	// It's ok for have a mutex here (and we may as well just to provide extra protection) because the close call should never block.
	m_pMutex->Lock();

	if (m_bPipeOpen)
	{
#ifdef WIN32
		HandleCloser(m_hRead);
		HandleCloser(m_hWrite);
#else

#ifdef UNIX_USE_PIPES
//		printf("[%5u] MpoPipe::Close %u and %u\n", getpid(), m_pipeRead, m_pipeWrite);
		if (close(m_pipeRead) != 0)
		{
//			printf("MpoPipe::Close failed: %d\n", errno);
		}
		if (close(m_pipeWrite) != 0)
		{
//			printf("MpoPipe::Close failed: %d\n", errno);
		}
#else
//		printf("[%5u] MpoPipe::Close %u\n", getpid(), m_socket);
		if (close(m_socket) != 0)
		{
#ifndef __APPLE__
			char buf[80];
//			printf("MpoPipe::Close failed: %s\n", strerror_r(errno, buf, sizeof(buf)));
#else
//			printf("MpoPipe::Close failed: %d\n", errno);
#endif // APPLE
		}
#endif // PIPES
		fflush(stdout);
#endif // UNIX

		if (uMsToWait != 0)
		{
			MpoTimerUtil::MakeDelay(uMsToWait);
		}

		m_bPipeOpen = false;
	}

	m_pMutex->Unlock();
}

////////////////

MpoPipe::MpoPipe()
{
	m_mutex = MpoThreadMutex::GetInstance();
	m_pMutex = m_mutex.get();
}

MpoPipe::~MpoPipe()
{
	Close();
}

//////////////////////////////////////////////////////////////

void MpoPipeExFactory::Create(IMpoPipeExSPtr &pipe1, IMpoPipeExSPtr &pipe2)
{
	if (!MpoPipeEx::Create(pipe1, pipe2))
	{
		throw runtime_error("Pipe creation failed");
	}
}

bool MpoPipeEx::Create(IMpoPipeExSPtr &pipeEx0, IMpoPipeExSPtr &pipeEx1)
{
	bool bRes = true;
	MpoPipeEx *pInstance[2];

	// create pipes for later use
	IMpoPipeSPtr pipe0, pipe1;
	if (!MpoPipe::Create(pipe0, pipe1))
	{
		return false;
	}

	pInstance[0] = new MpoPipeEx();

	if (!pInstance[0])
	{
		return false;
	}

	pInstance[1] = new MpoPipeEx();
	if (!pInstance[1])
	{
		return false;
	}

	pInstance[0]->m_pipeSPtr = pipe0;
	pInstance[0]->m_pPipe = pipe0.get();
	pInstance[1]->m_pipeSPtr = pipe1;
	pInstance[1]->m_pPipe = pipe1.get();

	MpoThreadMutexSPtr mutexes[2];

	// The reason we need two mutexes is so that both instances can write (or read) at the same time.
	// If we just had 1 mutex for everything, then only one instance could read or write at any time which is unnecessarily restrictive.
	mutexes[0] = MpoThreadMutex::GetInstance();
	mutexes[1] = MpoThreadMutex::GetInstance();

	for (int i = 0; i < 2;  i++)
	{
		pInstance[i]->m_mutexWSPtr = mutexes[i];
		pInstance[i]->m_pMutexW = mutexes[i].get();

		if (pInstance[i]->m_pMutexW == NULL)
		{
			bRes = false;
			break;
		}

		pInstance[i]->m_mutexRSPtr = mutexes[i^1];
		pInstance[i]->m_pMutexR = mutexes[i^1].get();

		if (pInstance[i]->m_pMutexR == NULL)
		{
			bRes = false;
			break;
		}

		// Set up variables so that different threads can query the value that another thread has
		pInstance[i]->m_iBufferSizeW = 0;
		pInstance[i]->m_stPendingBytesToBeWrittenW = 0;
		pInstance[i]->m_piBufferSizeR = &pInstance[i ^ 1]->m_iBufferSizeW;
		pInstance[i]->m_pstPendingBytesToBeWrittenR = &pInstance[i ^ 1]->m_stPendingBytesToBeWrittenW;
	}

	// if we haven't gotten any errors, then this function has succeeded
	if (bRes)
	{
		pipeEx0 = IMpoPipeExSPtr(pInstance[0], MpoPipeEx::deleter());
		pipeEx1 = IMpoPipeExSPtr(pInstance[1], MpoPipeEx::deleter());
	}
	// else we got an error, so delete the instances
	else
	{
		delete pInstance[0];
		delete pInstance[1];
	}

	return bRes;
}

size_t MpoPipeEx::GetReadBufferSize()
{
	int iRes = 0;

	m_pMutexR->Lock();
	iRes = *m_piBufferSizeR;
	m_pMutexR->Unlock();

	// this value can be negative but the caller doesn't need to know that (see notes in NonBlockingRead)
	if (iRes < 0) iRes = 0;

	return (size_t) iRes;
}

bool MpoPipeEx::BlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead)
{
	bool bRes = false;

	/*
	 * IMPORTANT!!!
	 * This BlockingReadInternal _must NOT_ be enclosed in a mutex because on linux (and probably other platforms),
	 *  it's possible for a thread to unlock and lock the mutex before another thread waiting
	 *  to acquire the mutex lock has got the lock.
	 * Therefore, with this in mind, any call which has the chance to block MUST
	 * not be inside a mutex.
	*/
	bRes = BlockingReadInternal(pBuf, stBytesToRead, pstBytesRead);

	if (bRes)
	{
//		printf("%x: BlockingRead About to lock %x\n", (int) pthread_self(), (int) m_pMutexR);

		// We must make sure that m_stBufferSize is only being read/written to by one thread/process at a time
		//    because it is used to tell whether BlockingRead would really block.
		// See NonBlockingRead for more comments.
		m_pMutexR->Lock();

//		printf("BlockingRead Locked %x\n", (int) m_pMutexR);

		// Our read count is the write count of our sibling class.
		// NOTE: this number can temporarily become negative if blockingread was called at one end of the pipe,
		//  and then blockingwrite was called on the other end of the pipe.
		// This is by design.
		*m_piBufferSizeR -= *pstBytesRead;

//		printf("(BlockingRead) BufferSize is %i\n", *m_piBufferSizeR);

		m_pMutexR->Unlock();

//		printf("BlockingRead Unlocked %x\n", (int) m_pMutexR);
	}

	return bRes;
}

bool MpoPipeEx::BlockingWrite(const void *pBuf, size_t stBytesToWrite, size_t *pstBytesWritten)
{
	bool bRes = false;

	// Blocking write MUST not grab the mutex before it writes because this could lead to a dead lock situation, if blocking read has already been called.
	// (because blocking read must grab the mutex before it reads as explained in nonblockingread function)

	// NOTE: on some platforms (maybe all?) this call will not return a partial result (it will either send the whole thing or block).
	// That is why we must track whether we have started the call using m_bWritePending.

	// must be enclosed in a mutex because it is shared across thread and NonblockingRead relies on it being up to date in order to make an informed decision.
	m_pMutexW->Lock();
	m_stPendingBytesToBeWrittenW = stBytesToWrite;
	m_pMutexW->Unlock();

	bRes = m_pPipe->BlockingWrite(pBuf, stBytesToWrite, pstBytesWritten);

	// must be enclosed in a mutex because it is shared across thread and NonblockingRead relies on it being up to date in order to make an informed decision.
	m_pMutexW->Lock();
	m_stPendingBytesToBeWrittenW = 0;
	m_pMutexW->Unlock();

	if (bRes)
	{
	///	printf("%x: BlockingWrite About to lock %x\n", (int) pthread_self(), (int) m_pMutexW);

		// We must make sure that m_stBufferSize is only being read/written to by one thread/process at a time,
		//  because it is used to tell whether BlockingRead would really block.
		// We do not need to wrap the mutex around the blocking write call because we only increase the value of m_stBufferSize,
		//  so we will never cause a non-blocking read to block.
		m_pMutexW->Lock();

	//	printf("BlockingWrite Locked %x\n", (int) m_pMutexW);
		m_iBufferSizeW += *pstBytesWritten;

	//	printf("(BlockingWrite) BufferSize is %i\n", m_iBufferSizeW);

		// At this point, the buffer size must always be >= 0; if it's negative, then we've got a critical error.
		// The reason this must be 0 or more is because if it's < 0 it means that more data was read from the pipe than was written to it which should be impossible.
		assert(m_iBufferSizeW >= 0);

		m_pMutexW->Unlock();

	//	printf("BlockingWrite Unlocked %x\n", (int) m_pMutexW);
	}
	else
	{
	//	printf("BlockingWrite failed\n");
	}

	return bRes;
}

bool MpoPipeEx::NonBlockingRead(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead)
{
	bool bRes = false;

	// We must make sure that m_stBufferSize is only being read/written to by one thread/process at a time
	//  because it is used to tell whether BlockingRead would really block.
	// This must be wrapped in a mutex because the value of m_stBufferSize must stay constant
	//  during this whole process because the logic here relies on that value (m_stBufferSize) being greater than zero.
	// (for example, calling BlockingRead relies on m_stBufferSize being non-zero, so we must guarantee that condition)
	m_pMutexR->Lock();

	// if read won't block
#ifdef WIN32
	// If the buffer is not empty, we know we won't block.
	// Also, if a write is pending and we know that the goal is to exceed a buffer size of 0, then we know reading won't block.
	// We must do both of these checks since a large BlockingWrite may not do a partial write but will block until the whole thing is finished,
	//   in which case our buffer size will show as <1 because it will not have been updated yet.
	if ((*m_piBufferSizeR > 0) || ((*m_piBufferSizeR + ((int) *m_pstPendingBytesToBeWrittenR)) > 0))
#else
	// this is necessary to solve the problem on unix (see notes in BlockingWrite)
	if (((MpoPipe *)m_pPipe)->WouldReadNotBlock())
#endif
	{
		// This is ONLY safe because we are confident that this call will NOT block.
		bRes = BlockingReadInternal(pBuf, stBytesToRead, pstBytesRead);		
		if (bRes)
		{
			// Our read count is the write count of our sibling class.
			// NOTE: this number can temporarily become negative if blockingread was called at one end of the pipe,
			//  and then blockingwrite was called on the other end of the pipe.
			// This is by design.
			*m_piBufferSizeR -= *pstBytesRead;

//			printf("(NonBlockingRead) BufferSize is %i\n", *m_piBufferSizeR);
		}

	}
	// else read would block, so just return immediately
	else
	{
		bRes = true;	// true means no error, not that data was returned
		*pstBytesRead = 0;	// no bytes were read because the buffer is empty
	}

	m_pMutexR->Unlock();

	return bRes;
}

/////

MpoPipeEx::MpoPipeEx()
{
}

MpoPipeEx::~MpoPipeEx()
{
	Close();
}

bool MpoPipeEx::BlockingReadInternal(void *pBuf, size_t stBytesToRead, size_t *pstBytesRead)
{
	bool bRes = m_pPipe->BlockingRead(pBuf, stBytesToRead, pstBytesRead);
	return bRes;
}

void MpoPipeEx::Close()
{
	m_pPipe->Close();
}
