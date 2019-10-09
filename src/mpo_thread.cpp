#include <mpolib2/mpo_thread.h>
#include <stdexcept>
#include <assert.h>

using namespace std;

bool mpo_create_thread(mpo_threadID *pThreadID, void *(*start_routine)(void*), void *pData)
{
	bool bRes = false;

#ifdef WIN32
	*pThreadID = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) start_routine, pData, 0, NULL);
	bRes = (*pThreadID != NULL);
#else
	int iRes = pthread_create(pThreadID, NULL, start_routine, pData);
	bRes = (iRes == 0);
#endif 

	return bRes;
}

bool mpo_wait_thread(const mpo_threadID *pThreadID)
{
	bool bRes = false;

#ifdef WIN32
	DWORD dwRes = WaitForSingleObject(*pThreadID, INFINITE);

	// if thread shutdown normally
	if (dwRes == WAIT_OBJECT_0)
	{
		// must call CloseHandle to free resources used by the thread
		bRes = (CloseHandle(*pThreadID) == TRUE);
	}
#else
	if (pthread_join(*pThreadID, NULL) == 0)
	{
		bRes = true;
	}
#endif // WIN32

	return bRes;
}

//////////////////////

MpoThreadMutexSPtr MpoThreadMutex::GetInstance()
{
	MpoThreadMutexSPtr pRes;
	MpoThreadMutex *pInstance = new MpoThreadMutex();
	if (pInstance->Init())
	{
		pRes = MpoThreadMutexSPtr(pInstance, MpoThreadMutex::deleter());
	}
	else
	{
		delete pInstance;
	}

	return pRes;
}

bool MpoThreadMutex::Lock()
{
	bool bRes = false;
	if (m_bInitialized)
	{

#ifdef WIN32
		EnterCriticalSection(&m_CritSect);

		if (m_stLockCount != 0)
		{
			int i = 0;
		}

		assert(m_stLockCount == 0);	// we don't support recursive locking
		m_stLockCount++;
		bRes = true;
#else
		if (pthread_mutex_lock(&m_Mutex) == 0)
		{
			assert(m_stLockCount == 0);	// we don't support recursive locking
			m_stLockCount++;
			bRes = true;
		}
		else
		{
			assert(false);	// we want to know about this
		}
#endif // WIN32
	}
	return bRes;
}

void MpoThreadMutex::LockEx()
{
	if (!Lock())
	{
		throw runtime_error("Mutex lock failed");
	}
}

bool MpoThreadMutex::Unlock()
{
	bool bRes = false;
	if (m_bInitialized)
	{
		assert(m_stLockCount > 0);
		if (m_stLockCount > 0)
		{
#ifdef WIN32
			m_stLockCount--;
			LeaveCriticalSection(&m_CritSect);
			bRes = true;
#else
			// decrement this before we unlock the mutex for sync purposes
			m_stLockCount--;
			assert(m_stLockCount == 0);
			if (pthread_mutex_unlock(&m_Mutex) == 0)
			{
				bRes = true;
			}
			else
			{
				assert(false);	// we want to know about this
				// else if we couldn't unlock, then restore the lock count to what it was before (this should never happen)
				m_stLockCount++;
			}
#endif // WIN32
		} // end if lockcount is ok
	}
	return bRes;
}

void MpoThreadMutex::UnlockEx()
{
	if (!Unlock())
	{
		throw runtime_error("Mutex unlock failed");
	}
}

/////////////////

MpoThreadMutex::MpoThreadMutex() :
m_stLockCount(0),
m_bInitialized(false)
{
}

MpoThreadMutex::~MpoThreadMutex()
{
	Shutdown();
}

bool MpoThreadMutex::Init()
{
	bool bRes = false;

	if (!m_bInitialized)
	{
#ifdef WIN32
		InitializeCriticalSection(&m_CritSect);
		m_bInitialized = true;
		bRes = true;
#else
		int i = pthread_mutex_init(&m_Mutex, NULL);
		if (i == 0)
		{
			bRes = m_bInitialized = true;
		}
		// else handle error code (this probably won't ever happen)

#endif // WIN32
	}

	return bRes;
}

void MpoThreadMutex::Shutdown()
{
	if (m_bInitialized)
	{
#ifdef WIN32
		DeleteCriticalSection(&m_CritSect);
#else
		pthread_mutex_destroy(&m_Mutex);
#endif // WIN32
		m_bInitialized = false;
	}
}

///////////////////////////////////

MpoThreadMutexSafeSPtr MpoThreadMutexSafe::SafeLock(MpoThreadMutex *pMutex)
{
	MpoThreadMutexSafeSPtr pRes;
	MpoThreadMutexSafe *pInstance = new MpoThreadMutexSafe(pMutex);

	if (pInstance)
	{
		if (pInstance->Init())
		{
			pRes = MpoThreadMutexSafeSPtr(pInstance, MpoThreadMutexSafe::deleter());
		}
		else
		{
			delete pInstance;
		}
	}

	return pRes;
}

MpoThreadMutexSafeSPtr MpoThreadMutexSafe::SafeLockEx(MpoThreadMutex *pMutex)
{
	MpoThreadMutexSafeSPtr pRes = SafeLock(pMutex);

	if (pRes.get() == NULL)
	{
		throw runtime_error("Safe mutex lock failed");
	}

	return pRes;
}

MpoThreadMutexSafe::MpoThreadMutexSafe(MpoThreadMutex *pMutex) :
m_pMutex(pMutex),
m_bLocked(false)
{
}

MpoThreadMutexSafe::~MpoThreadMutexSafe()
{
	Shutdown();
}

bool MpoThreadMutexSafe::Init()
{
	m_bLocked = m_pMutex->Lock();
	return m_bLocked;
}

void MpoThreadMutexSafe::Shutdown()
{
	if (m_bLocked)
	{
		m_pMutex->Unlock();
	}
}
