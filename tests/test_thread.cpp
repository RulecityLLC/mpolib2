#include "test_headers.h"

typedef struct test_thread_comm_s
{
	MpoThreadMutex *pMutex;
	bool bChildLocked;
	bool bChildDone;
	bool bChildError;
} test_thread_comm_t;

void *child_thread1(void *pParm)
{
	test_thread_comm_t *pComm = (test_thread_comm_t *) pParm;
	MpoThreadMutex *pMutex = pComm->pMutex;

	bool bRes = pMutex->Lock();
	if (bRes)
	{
		pComm->bChildLocked = true;

		// sleep for a little while so parent can verify that it had to wait to get the mutex lock
		make_delay(250);
		bRes = pMutex->Unlock();
		if (bRes != true) pComm->bChildError = true;
		pComm->bChildLocked = false;
	}
	else
	{
		pComm->bChildError = true;
	}

	pComm->bChildDone = true;
	return 0;
}

void test_thread1()
{
	MpoThreadMutexSPtr MutexSPtr = MpoThreadMutex::GetInstance();
	MpoThreadMutex *pMutex = MutexSPtr.get();
	TEST_REQUIRE(pMutex != 0);

	test_thread_comm_t comm;
	comm.pMutex = pMutex;
	comm.bChildLocked = false;
	comm.bChildDone = false;
	comm.bChildError = false;

	mpo_threadID threadID;
	mpo_create_thread(&threadID, child_thread1, &comm);

	// wait for child to lock
	while (comm.bChildLocked == false)
	{
		make_delay(1);
	}

	unsigned int uTimeMs = refresh_timer();
	bool bRes = pMutex->Lock();
	TEST_REQUIRE(bRes);	// this should always return true because Lock() won't return until a lock has been achieved

	unsigned int uElapsedMs = get_elapsed_ms(uTimeMs);
	TEST_CHECK(uElapsedMs > 125);	// we should've waited at least this long

	bRes = mpo_wait_thread(&threadID);
	TEST_CHECK(bRes);

	TEST_CHECK(comm.bChildDone);
}

TEST_CASE(thread1)
{
	test_thread1();
}

void test_safemutex1()
{
	MpoThreadMutexSPtr mutexSPtr = MpoThreadMutex::GetInstance();
	MpoThreadMutex *pMutex = mutexSPtr.get();
	try
	{
		MpoThreadMutexSafeSPtr mutexSafeSPtr = MpoThreadMutexSafe::SafeLockEx(pMutex);

		// test to make sure the mutex gets unlocked when an exception forces it out of scope
		throw false;
	}
	catch (...)
	{
	}

	// if all went well the mutex should be unlocked now

	// these functions should succeed if the mutex is unlocked at this point
	pMutex->LockEx();
	pMutex->UnlockEx();
}

TEST_CASE(safemutex1)
{
	test_safemutex1();
}
