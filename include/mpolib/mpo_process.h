#ifndef MPO_PROCESS_H
#define MPO_PROCESS_H

#include "mpo_dll.h"
#include "mpo_deleter.h"
#include "mpo_thread.h"
#include "mpo_ipc.h"

#include <list>
#include <string>
using namespace std;

// The purpose of this interface is so we can mock out MpoProcess

class IMpoProcess
{
public:
	// WAITR_* prefix used so as to not conflict with definitions in windows.h
	typedef enum
	{
		WAITR_ERROR, WAITR_FINISHED, WAITR_BUSY,

		// this one means the child process never launched but we weren't able to determine that until waiting (unix limitation)
		WAITR_NEVER_LAUNCHED
	} waitr;

	// Returns true if launch was requested successfully (does NOT mean that process actually started; due to limitation in unix, caller must call Wait to determine whether process ran correctly)
	// On unix, the arguments will be converted to UTF8!!! (unix doesn't support wide command lines)
	virtual bool Launch(const list <wstring> &cmd_line, bool bCaptureStdOut = false, bool bCaptureStdIn = false, bool bCaptureStdErr = false) = 0;

	virtual bool Launch(const list <string> &cmd_line, bool bCaptureStdOut = false, bool bCaptureStdIn = false, bool bCaptureStdErr = false) = 0;

	virtual waitr Wait(int *exit_code, unsigned int uTimeoutMs) = 0;

	// Set 'bNice' to true if you expect the process to shutdown gracefully (this call will block briefly as it waits for the process to shut down)
	// Set 'bNice' to false if you don't expect the process to shutdown gracefully (ie you want to forcefully kill the process)
	// Returns true if signal was successfully sent to process.
	virtual bool RequestShutdown(bool bNice = true) = 0;

	// We used to use MpoPipeEx for std i/o but had too many random problems so switched to a safer method.
	// If 'bFinal' is true, it means that all remaining data from the stream in question will be read before the function returns.
	// These functions may throw exceptions on error.
	virtual string ReadStdOutEx(bool bFinal = false) = 0;
	virtual string ReadStdErrEx(bool bFinal = false) = 0;

	// This call may block if the process cannot accept the input.
	// This call may throw an exception.
	virtual void WriteStdInEx(const void *pBuf, size_t stBufSizeBytes) = 0;

	// If child process is reading from stdin, this is a clean way for us to signal it to exit without having to forcefully kill it.
	// (throws exception on error)
	virtual void CloseStdIOThreads() = 0;

};

class MpoProcess;

typedef shared_ptr<MpoProcess> MpoProcessSPtr;

class EXPORT_ME MpoProcess : public IMpoProcess, public MpoDeleter
{
public:
	static MpoProcessSPtr GetInstance();

	// helper function to convert windows command line into list
	// (used by mpo_process_dotnet but I thought it belonged here so it would be unit tested)
	static list<string> unescape_cmdline(const string &strCmdLine);

	// wide version
	static list<wstring> unescape_cmdline(const wstring &strCmdLine);

#ifdef WIN32
	// helper function to handle quotations around command line arguments
	static wstring escape_arg(const wstring &arg);
	static string escape_arg(const string &arg);

	template <class _si> bool SetStartupInfoTemplate(_si &si, bool bCaptureStdOut, bool bCaptureStdIn, bool bCaptureStdErr);
#endif // WIN32
	template <class _ch> bool LaunchTemplate(const list <basic_string<_ch> > &cmd_line, bool bCaptureStdOut = false, bool bCaptureStdIn = false, bool bCaptureStdErr = false);

	// Returns true if launch was requested successfully (does NOT mean that process actually started; due to limitation in unix, caller must call Wait to determine whether process ran correctly)
	bool Launch(const list <wstring> &cmd_line, bool bCaptureStdOut = false, bool bCaptureStdIn = false, bool bCaptureStdErr = false);

	bool Launch(const list <string> &cmd_line, bool bCaptureStdOut = false, bool bCaptureStdIn = false, bool bCaptureStdErr = false);

	waitr Wait(int *exit_code, unsigned int uTimeoutMs);

	bool RequestShutdown(bool bNice = true);

	string ReadStdOutEx(bool bFinal = false);
	string ReadStdErrEx(bool bFinal = false);
	void WriteStdInEx(const void *pBuf, size_t stBufSizeBytes);

	void CloseStdIOThreads();

private:
	MpoProcess();
	~MpoProcess();

	void DeleteInstance();

	// call this if you don't expect the process to shutdown gracefully before Shutdown() is called
	bool Kill();

	void Shutdown();

	void SetError(const string &strLastErr);

	static void AddToBuf(MpoProcess *pProc, string *pStrDstBuf, const void *pSrcBuf, size_t stSrcBufSizeBytes);
	static void *StdOutThread(void *pInstance);
	static void *StdErrThread(void *pInstance);

	//////

	MpoThreadMutexSPtr m_MutexSPtr;
	MpoThreadMutex *m_pMutex;

	// whether the process is running (ie if it needs to be forcefully terminated)
	bool m_bProcessRunning;

	// whether the child threads should be running
	// (this is separate from whether the process is running because the child threads still may need to gather pipe data after the process has exited)
	bool m_bThreadsShouldBeRunning;

	// so parent thread can tell children that it knows they are running
	// (this is mainly to prevent the case of Launch getting called and shutdown immediately getting called right afterwards)
	bool m_bParentAckChildren;

	bool m_bEOF;
	string m_strLastErr;

	// Whether associated threads are still running.
	// (we can't let this class be destroyed until these threads are all stopped).
	// These variables should only be writable by the child threads themselves.
	bool m_bStdOutRunning, m_bStdErrRunning;

	// Whether parent thread needs to "join" the child thread (ie wait for child thread to exit).
	// This is necessary to clean up resources used by the thread's creation.
	// These variables should only be writable by the parent thread.
	bool m_bStdOutNeedsJoin, m_bStdErrNeedsJoin;

	// thread ID's
	mpo_threadID m_threadOut, m_threadErr;

	// String buffers that hold what we've sent/received from the process.
	// These must be guarded by a mutex since at least two threads will access them.
	string m_strStdOut;
	string m_strStdErr;

#ifdef WIN32
	PROCESS_INFORMATION m_pi;
	HANDLE m_hStdOutR, m_hStdOutW;
	HANDLE m_hStdErrR, m_hStdErrW;
	HANDLE m_hStdInR, m_hStdInW;
#else
	pid_t m_pid;
	int m_iStdOut[2];
	int m_iStdErr[2];
	int m_iStdIn[2];
#endif // WIN32

};

#endif // MPO_PROCESS_H
