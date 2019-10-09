#include "test_headers.h"

void test_process1()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	// call wait when no process is running
	MpoProcess::waitr w = pProcess->Wait(NULL, 0);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_ERROR, w);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_helper.exe");
#else
	lstCmdLine.push_back("./std_test_helper");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, true, true, true);
	TEST_REQUIRE(bRes);
	string s;

	// we should get an exception when trying to do a final read while the process is running
	TEST_CHECK_THROW(pProcess->ReadStdOutEx(true));
	TEST_CHECK_THROW(pProcess->ReadStdErrEx(true));

	for (;;)
	{
		s = pProcess->ReadStdOutEx();
		if (!s.empty())
		{
			break;
		}
		make_delay(1);
	}
	TEST_CHECK_EQUAL("hi", s);

//printf("test_process 1 about to write\n");

	s = "whatever";
	pProcess->WriteStdInEx(s.data(), s.size());

//printf("test_process1 about to read from stderr\n");

	for (;;)
	{
		s = pProcess->ReadStdErrEx();
		if (!s.empty())
		{
			break;
		}
		make_delay(1);
	}
	TEST_CHECK_EQUAL("error", s);

	s.clear();

//printf("test_process1 about to read from stdout in loop\n");

	// wait for child process to write 5 bytes to stdout
	while (s.size() < 5)
	{
		string s1 = pProcess->ReadStdOutEx();
		s += s1;
		make_delay(1);
	}
	TEST_CHECK_EQUAL("there", s);

//printf("test_process1 about to wait for child process to exit\n");

	// give other process a chance to close its stdin pipe
	make_delay(1);
	s = "this should fail";
	TEST_CHECK_THROW(pProcess->WriteStdInEx(s.data(), s.size()));

	// now if we try to write to stdin again, we should get a different exception about the pipe being closed
	try
	{
		pProcess->WriteStdInEx(s.data(), s.size());
		TEST_REQUIRE(false);	// if we get this far, we didn't get the exception like we should've
	}
	catch (std::exception &ex)
	{
		TEST_CHECK(string(ex.what()) == string("StdIn pipe is closed."));
	}

	int iExitCode = 0;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_FINISHED, res);

	// the process should've returned this code
	TEST_CHECK_EQUAL(11, iExitCode);

//	printf("test_process1 done\n");
}

TEST_CASE(process1)
{
	test_process1();
}

void test_process1kill()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_helper.exe");
#else
	lstCmdLine.push_back("./std_test_helper");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, true, true, true);
	TEST_REQUIRE(bRes);
	string s;

	// wait for child process to write to stdout so we know that it's running
	for (;;)
	{
		s = pProcess->ReadStdOutEx();
		if (!s.empty())
		{
			break;
		}
		make_delay(1);
	}

	// kill the process forcefully
	bRes = pProcess->RequestShutdown(false);
	TEST_CHECK(bRes);
}

TEST_CASE(process1kill)
{
	test_process1kill();
}

void test_process2()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_helper.exe");
#else
	lstCmdLine.push_back("./std_test_helper");
#endif

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, true, true, true);
	TEST_REQUIRE(bRes);

	// exit without waiting for the process to finish (it won't finish because we haven't passed it stdin)
}

TEST_CASE(process2)
{
	test_process2();
}

void test_process3()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_helper.exe");
#else
	lstCmdLine.push_back("./std_test_helper");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, false, true, true);
	TEST_REQUIRE(bRes);

	// send to stdin so process can exit
	string s = "abc";
	pProcess->WriteStdInEx(s.data(), s.size());

	int iExitCode = 0;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_FINISHED, res);

	// the process should've returned this code
	TEST_CHECK_EQUAL(11, iExitCode);

	// make sure we can read stderr after process has exited
	s = pProcess->ReadStdErrEx(true);
	TEST_CHECK_EQUAL("error", s);
}

TEST_CASE(process3)
{
	test_process3();
}

/////////////////////////////////////////////////////////////////////

void test_process_exiter()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_exiter.exe");
#else
	lstCmdLine.push_back("./std_test_exiter");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, false, true, false);
	TEST_REQUIRE(bRes);

	// go until we get an exception due to the stdin thread no longer running
	try
	{
		for (;;)
		{
			// send to stdin to the exited process
			string s = "abc";
			pProcess->WriteStdInEx(s.data(), s.size());
		}
	}
	catch (std::exception &)
	{
	}

	int iExitCode = 0;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_FINISHED, res);

	// the process should've returned this code
	TEST_CHECK_EQUAL(17, iExitCode);
}

TEST_CASE(process_exiter)
{
	test_process_exiter();
}

////

void test_process_exiter2()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_exiter.exe");
#else
	lstCmdLine.push_back("./std_test_exiter");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, true, false, false);
	TEST_REQUIRE(bRes);

	int iExitCode = 0;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_FINISHED, res);

	// we should have some stuff after the process has exited
	string s = pProcess->ReadStdOutEx(true);
	TEST_CHECK_EQUAL(100000, s.size());

	// the process should've returned this code
	TEST_CHECK_EQUAL(17, iExitCode);
}

TEST_CASE(process_exiter2)
{
	test_process_exiter2();
}

void test_process_exiter3()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_exiter.exe");
#else
	lstCmdLine.push_back("./std_test_exiter");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, false, false, true);
	TEST_REQUIRE(bRes);

	int iExitCode = 0;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_FINISHED, res);

	string s = pProcess->ReadStdErrEx(true);
	TEST_CHECK_EQUAL(0, s.size());

	// the process should've returned this code
	TEST_CHECK_EQUAL(17, iExitCode);
}

TEST_CASE(process_exiter3)
{
	test_process_exiter3();
}

void test_process_exiter4()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_exiter.exe");
#else
	lstCmdLine.push_back("./std_test_exiter");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, true, false, false);
	TEST_REQUIRE(bRes);

	// try writing to the process's stdin even though we're not capturing stdin.  Should get an exception.
	TEST_CHECK_THROW(pProcess->WriteStdInEx(&bRes, sizeof(bRes)));

	bool bProcessRunning = true;
	int iExitCode = 0;
	size_t stTotalBytes = 0;

	// go until we fill up the buffer
	while (bProcessRunning)
	{
		MpoProcess::waitr res = pProcess->Wait(&iExitCode, 0);
		if (res != MpoProcess::WAITR_BUSY)
		{
			bProcessRunning = false;
		}

		string s = pProcess->ReadStdOutEx();
		stTotalBytes += s.size();
		make_delay(1);
	}

	// now process isn't running anymore, we should be able to read the remainder
	string s = pProcess->ReadStdOutEx(true);
	stTotalBytes += s.size();

	TEST_CHECK_EQUAL(100000, stTotalBytes);

	// the process should've returned this code
	TEST_CHECK_EQUAL(17, iExitCode);
}

TEST_CASE(process_exiter4)
{
	test_process_exiter4();
}

void test_process_stdin()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	// try writing to the process's stdin before we've launched.  We should get an exception.
	TEST_CHECK_THROW(pProcess->WriteStdInEx(&bRes, sizeof(bRes)));

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("std_test_wait_for_stdin.exe");
#else
	lstCmdLine.push_back("./std_test_wait_for_stdin");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	bRes = pProcess->Launch(lstCmdLine, false, true, false);
	TEST_REQUIRE(bRes);

	// give stdin thread a chance to go to sleep
	make_delay(1);

	// close all pipes which means process _should_ exit immediately
	pProcess->CloseStdIOThreads();

	int iExitCode = -1;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_REQUIRE_EQUAL(MpoProcess::WAITR_FINISHED, res);

	// the process should've returned this code
	TEST_CHECK_EQUAL(0, iExitCode);

	// Try writing to stdin after the process has exited.  We should get an exception.
	TEST_CHECK_THROW(pProcess->WriteStdInEx(&bRes, sizeof(bRes)));
}

TEST_CASE(process_stdin)
{
	test_process_stdin();
}

void test_process_detach()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<string> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back("test_exit_after_10.exe");
#else
	lstCmdLine.push_back("./test_exit_after_10");
#endif // 

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(mpom::str_conv(lstCmdLine.front().c_str())));

	// general use case for "detaching" is that the child process's stdio is completely independent of ours
	bRes = pProcess->Launch(lstCmdLine, false, false, false);
	TEST_REQUIRE(bRes);

	// Destroy the process instance without waiting for child process to exit.
	// This should be a supported use case.
	// We will know if it failed because the MpoProcess code would have an assertion error before it was fixed.
	ProcSPtr.reset();
}

TEST_CASE(process_detach)
{
	test_process_detach();
}

void test_process_wide()
{
	bool bRes = false;
	MpoProcessSPtr ProcSPtr = MpoProcess::GetInstance();
	MpoProcess *pProcess = ProcSPtr.get();

	TEST_REQUIRE(pProcess != 0);

	list<wstring> lstCmdLine;
#ifdef WIN32
	lstCmdLine.push_back(L"test_wide_helper.exe");
#else
	lstCmdLine.push_back(L"./test_wide_helper");
#endif //

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	// make sure executable exists in current directory
	TEST_REQUIRE(pFileIO->FileExists(lstCmdLine.front().c_str()));

	wstring wstrTmpFile = L"\x65e5\x672c\x8a9e";
	blocking_sharedptr stream = MpoFileStreamFactory::CreateInstance(wstrTmpFile, MPO_OPEN_CREATE);
	TEST_REQUIRE(stream.get());	// make sure file was created
	stream.reset();

	lstCmdLine.push_back(wstrTmpFile);
	bRes = pProcess->Launch(lstCmdLine, false, false, false);
	TEST_REQUIRE(bRes);

	int iExitCode = 0;
	MpoProcess::waitr res = pProcess->Wait(&iExitCode, 5000);
	TEST_CHECK_EQUAL(MpoProcess::WAITR_FINISHED, res);

	// the process should've returned this code
	TEST_CHECK_EQUAL(123, iExitCode);

	pFileIO->Delete(wstrTmpFile.c_str());
}

TEST_CASE(process_wide)
{
	test_process_wide();
}

void test_unescape_cmdline()
{
	string s = "hi\".txt\"";
	list<string> lstRes;
	lstRes = MpoProcess::unescape_cmdline(s);

	TEST_CHECK_EQUAL(1, lstRes.size());
	TEST_CHECK_EQUAL("hi.txt", lstRes.front());

	s = "\"i have a space\" in me";
	lstRes = MpoProcess::unescape_cmdline(s);

	TEST_CHECK_EQUAL(3, lstRes.size());
	TEST_CHECK_EQUAL("i have a space", lstRes.front());

	wstring ws = L"\"i have a space\" in me";
	list<wstring> lstResW;
	lstResW = MpoProcess::unescape_cmdline(ws);

	TEST_CHECK_EQUAL(3, lstResW.size());
	TEST_CHECK(L"i have a space" == lstResW.front());
}

TEST_CASE(unescape_cmdline)
{
	test_unescape_cmdline();
}
