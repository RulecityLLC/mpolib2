#include "test_headers.h"

const char msg[] = "Hi there, this is the message!";
const char msg2[] = "This is the response";

void *child_thread_launcher(void *pParm)
{
	char buf[160];
	IMpoPipe *pPipe = (IMpoPipe *) pParm;
	size_t stBytesWritten = 0;
	pPipe->BlockingWrite(msg, sizeof(msg), &stBytesWritten);

	size_t stBytesRead = 0;
	pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);

	// one last time so parent thread knows we received the message
	pPipe->BlockingWrite(msg, sizeof(msg), &stBytesWritten);

	return 0;
}

void test_pipe1()
{
	IMpoPipeSPtr pipe0, pipe1;
	MpoPipeFactory::Create(pipe0, pipe1);

	mpo_threadID threadID;
	mpo_create_thread(&threadID, child_thread_launcher, pipe1.get());

	IMpoPipe *pPipe = pipe0.get();
	char buf[160];
	size_t stBytesRead, stBytesWritten;
	bool bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);

	string s = buf;
	string s1 = msg;
	TEST_CHECK_EQUAL(s, s1);

	bRes = pPipe->BlockingWrite(msg2, sizeof(msg2), &stBytesWritten);
	TEST_CHECK(bRes);

	bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);

	mpo_wait_thread(&threadID);

	s = buf;
	TEST_CHECK_EQUAL(s, s1);
}

TEST_CASE(pipe1)
{
	test_pipe1();
}

///////////////

void *child_thread_launcher_ex(void *pParm)
{
	char buf[160];
	const size_t uBig = 1024 * 1024;
	char *pHugebuf = (char *) malloc(uBig);	// to mimic problem I found on OSX
	IMpoPipeEx *pPipe = (IMpoPipeEx *) pParm;
	size_t stBytesWritten = 0;

	pPipe->BlockingWrite(msg, sizeof(msg), &stBytesWritten);

	size_t stBytesRead = 0;
	pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);

	// one last time so parent thread knows we received the message
	// Since we write a huge chunk, this also tests to make sure that are nonblockingread routine can handle it.
	pPipe->BlockingWrite(pHugebuf, uBig, &stBytesWritten);

	free(pHugebuf);

	return 0;
}

void test_pipe_ex()
{
	IMpoPipeExSPtr pipe0, pipe1;
	MpoPipeExFactory::Create(pipe0, pipe1);

	mpo_threadID threadID;
	mpo_create_thread(&threadID, child_thread_launcher_ex, pipe1.get());

	IMpoPipeEx *pPipe = pipe0.get();
	char buf[160];
	size_t stBytesRead = 0, stBytesWritten = 0;

	bool bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);

	string s = buf;
	string s1 = msg;
	TEST_CHECK_EQUAL(s, s1);

	// we should read no bytes until we've written something, but this tests the non-blocking reader
	for (int i = 0; i < 3; i++)
	{
		bRes = pPipe->NonBlockingRead(buf, sizeof(buf), &stBytesRead);
	}
	TEST_CHECK_EQUAL(0, stBytesRead);

	bRes = pPipe->BlockingWrite(msg2, sizeof(msg2), &stBytesWritten);
	TEST_CHECK(bRes);

	size_t stTotalBytesRead = 0;

	// wait until we start reading stuff
	while (stBytesRead == 0)
	{
		bRes = pPipe->NonBlockingRead(buf, sizeof(buf), &stBytesRead);
		stTotalBytesRead += stBytesRead;
	}

	// now read all of it so that the child thread can unblock
	while (stTotalBytesRead < (1024 *1024))
	{
		bRes = pPipe->NonBlockingRead(buf, sizeof(buf), &stBytesRead);
		stTotalBytesRead += stBytesRead;
	}

	TEST_CHECK(bRes);

	TEST_CHECK_EQUAL(1024 * 1024, stTotalBytesRead);

	// make sure child thread exits (not blocked)
	mpo_wait_thread(&threadID);
}

TEST_CASE(pipe_ex)
{
	test_pipe_ex();
}

///////////////////////

void *child_thread_launcher_close(void *pParm)
{
	char buf[160];
	IMpoPipe *pPipe = (IMpoPipe *) pParm;
	size_t stBytesWritten = 0;
	pPipe->BlockingWrite(msg, sizeof(msg), &stBytesWritten);

	size_t stBytesRead = 0;
	pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);

	// see if writing to the pipe and then closing it yields expected result
	pPipe->BlockingWrite(msg, sizeof(msg), &stBytesWritten);

	// close pipe
	pPipe->Close();

	return 0;
}

void test_pipe_close()
{
	IMpoPipeSPtr pipe0, pipe1;
	MpoPipeFactory::Create(pipe0, pipe1);

	mpo_threadID threadID;
	mpo_create_thread(&threadID, child_thread_launcher_close, pipe1.get());

	IMpoPipe *pPipe = pipe0.get();
	char buf[160];
	size_t stBytesRead, stBytesWritten;
	bool bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);

	string s = buf;
	string s1 = msg;
	TEST_CHECK_EQUAL(s, s1);

	bRes = pPipe->BlockingWrite(msg2, sizeof(msg2), &stBytesWritten);
	TEST_CHECK(bRes);

	bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);

	// this should fail because the pipe will be closed
	bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(!bRes);

	mpo_wait_thread(&threadID);

	// now send the pipes in the reverse order
	MpoPipeFactory::Create(pipe0, pipe1);
	mpo_create_thread(&threadID, child_thread_launcher_close, pipe0.get());
	pPipe = pipe1.get();
	bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);
	s = buf;
	s1 = msg;
	TEST_CHECK_EQUAL(s, s1);
	bRes = pPipe->BlockingWrite(msg2, sizeof(msg2), &stBytesWritten);
	TEST_CHECK(bRes);

	bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(bRes);

	// this should fail because the pipe will be closed
	bRes = pPipe->BlockingRead(buf, sizeof(buf), &stBytesRead);
	TEST_CHECK(!bRes);	
}

TEST_CASE(pipe_close)
{
	test_pipe_close();
}

///////////////////////////////////////////////////////////////////////////////

bool g_bOkToClose = false;
void *child_thread_closer(void *pParm)
{
	IMpoPipe *pPipe = (IMpoPipe *) pParm;

	g_bOkToClose = true;

	// close pipe with some delay to give the other thread a chance to close the same pipe
	pPipe->Close(1000);

	return 0;
}

void test_pipe_two_threads_closing()
{
	IMpoPipeSPtr pipe0, pipe1;
	MpoPipeFactory::Create(pipe0, pipe1);

	mpo_threadID threadID;
	mpo_create_thread(&threadID, child_thread_closer, pipe0.get());

	// wait for other thread to start closing
	while (!g_bOkToClose)
	{
		MpoTimerUtil::MakeDelay(1);
	}

	// now close pipe0 before the thread has closed it
	pipe0->Close();

	// visual studio will throw an exception when run in debug mode if there is a problem here

	// wait for the thread to return before we destroy pipe0, otherwise it will crash
	mpo_wait_thread(&threadID);

	// clean-up
	pipe0.reset();
	pipe1.reset();
}

TEST_CASE(pipe_two_threads_closing)
{
	test_pipe_two_threads_closing();
}
