#ifndef MPO_THREAD_H
#define MPO_THREAD_H

#include "mpo_dll.h"
#include "mpo_deleter.h"

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
typedef HANDLE mpo_threadID ;

// dll-interface warning about private member variable
#pragma warning(disable: 4251)
#else
#include <pthread.h>
typedef pthread_t mpo_threadID;
#endif // WIN32

EXPORT_ME bool mpo_create_thread(mpo_threadID *pThreadID, void *(*start_routine)(void*), void *pData);

// Blocks until the specified thread terminates.
// Returns false if there is an error such as the thread ID being unknown.
EXPORT_ME bool mpo_wait_thread(const mpo_threadID *pThreadID);

class MpoThreadMutex;

typedef shared_ptr<MpoThreadMutex> MpoThreadMutexSPtr;

class EXPORT_ME MpoThreadMutex : public MpoDeleter
{
public:
	static MpoThreadMutexSPtr GetInstance();

	bool Lock();

	// same as Lock() but throws an exception on failulre
	void LockEx();

	bool Unlock();

	// same as Unlock() but throws an exception on failulre
	void UnlockEx();

private:
	MpoThreadMutex();
	virtual ~MpoThreadMutex();

	void DeleteInstance() { delete this; }

	bool Init();
	void Shutdown();

	size_t m_stLockCount;
	bool m_bInitialized;

#ifdef WIN32
	CRITICAL_SECTION m_CritSect;
#else
	pthread_mutex_t m_Mutex;
#endif // WIN32
};

////////////////////////////////////////////////////////////////////////////////

// The purpose of this little class is so that we can guarantee that the mutex that we lock also gets unlocked in the case
//  of an unforeseen event (like an exception) taking us out of the program flow before we can unlock the mutex.

class MpoThreadMutexSafe;

typedef shared_ptr<MpoThreadMutexSafe> MpoThreadMutexSafeSPtr;

class EXPORT_ME MpoThreadMutexSafe : public MpoDeleter
{
public:
	static MpoThreadMutexSafeSPtr SafeLock(MpoThreadMutex *pMutex);

	// same as previous function except throws an exception on failure
	static MpoThreadMutexSafeSPtr SafeLockEx(MpoThreadMutex *pMutex);

private:
	MpoThreadMutexSafe(MpoThreadMutex *pMutex);
	~MpoThreadMutexSafe();

	void DeleteInstance() { delete this; }

	bool Init();
	void Shutdown();

	MpoThreadMutex *m_pMutex;
	bool m_bLocked;
};

#endif // MPO_THREAD_H
