#include <mpolib2/mpo_process.h>
#include <mpolib2/mpo_numstr.h>
#include <mpolib2/mpo_timer.h>
#include <mpolib2/mpo_misc.h>	// for unicode conversion
#include <stdexcept>

#ifndef WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <string.h>
#include <errno.h>

#endif //not windows

MpoProcessSPtr MpoProcess::GetInstance()
{
	MpoProcessSPtr pRes;
	MpoProcess *pInstance = new MpoProcess();

	pInstance->m_MutexSPtr = MpoThreadMutex::GetInstance();
	pInstance->m_pMutex = pInstance->m_MutexSPtr.get();
	
	if (pInstance->m_pMutex)
	{
		pRes = MpoProcessSPtr(pInstance, MpoProcess::deleter());
	}
	else
	{
		delete pInstance;
	}
	
	return pRes;
}

template <class _ch> list<basic_string<_ch> > unescape_cmdline_template(const basic_string<_ch> &strCmdLine)
{
	list<basic_string<_ch> > lstRes;
	bool bInQuote = false;
	basic_string<_ch> strCurArg;

	const _ch *pSrc = strCmdLine.c_str();

	while (*pSrc != 0)
	{
		_ch ch = *pSrc;
		switch (ch)
		{
		case '"':
			bInQuote = !bInQuote;
			break;
		case ' ':
			// if this is the end of an argument
			if (!bInQuote)
			{
				lstRes.push_back(strCurArg);
				strCurArg.clear();
			}
			else
			{
				strCurArg += ch;
			}
			break;
		default:
			strCurArg += ch;
			break;
		}
		pSrc++;
	}

	// add the final argument
	if (!strCurArg.empty())
	{
		lstRes.push_back(strCurArg);
		strCurArg.clear();
	}

	return lstRes;
}

list<string> MpoProcess::unescape_cmdline(const string &strCmdLine)
{
	return unescape_cmdline_template(strCmdLine);
}

list<wstring> MpoProcess::unescape_cmdline(const wstring &strCmdLine)
{
	return unescape_cmdline_template(strCmdLine);
}

#ifdef WIN32
template <class _ch> basic_string<_ch> escape_arg_template(const basic_string<_ch> &arg)
{
	basic_string<_ch> strResult;
	bool bFoundSpace = false;

	for (typename basic_string<_ch>::const_iterator si = arg.begin(); si != arg.end(); si++)
	{
		_ch ch = *si;
		// if it's not a quote
		if (ch != '"')
		{
			strResult += ch;

			// if it's a space, the whole thing will need to be escaped
			if (ch == ' ') bFoundSpace = true;
		}
		// else ignore the quote
	}
	if (bFoundSpace)
	{
		strResult.insert(0, 1, '"');
		strResult.append(1, '"');
	}
	return strResult;
}

wstring MpoProcess::escape_arg(const wstring &arg)
{
	return escape_arg_template(arg);
}

string MpoProcess::escape_arg(const string &arg)
{
	return escape_arg_template(arg);
}

void CloseHandleCheck(HANDLE &h)
{
	if (h != (HANDLE) -1)
	{
		CloseHandle(h);
		h = (HANDLE) -1;
	}
}

template <class _si> bool MpoProcess::SetStartupInfoTemplate(_si &si, bool bCaptureStdOut, bool bCaptureStdIn, bool bCaptureStdErr)
{
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	// initialize for default case
	si.hStdOutput = si.hStdInput = si.hStdError = (HANDLE) -1;
	si.dwFlags |= STARTF_USESTDHANDLES;

	// for capturing std handles
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL;

	if (bCaptureStdOut)
	{
		if (! CreatePipe(&m_hStdOutR, &m_hStdOutW, &saAttr, 0))
		{
			SetError("CreatePipe failed for stdout");
			return false;
		}

		// read handle must not be inherited
		SetHandleInformation( m_hStdOutR, HANDLE_FLAG_INHERIT, 0);
		si.hStdOutput = m_hStdOutW;
	}

	if (bCaptureStdErr)
	{
		if (! CreatePipe(&m_hStdErrR, &m_hStdErrW, &saAttr, 0))
		{
			SetError("CreatePipe failed for stderr");
			return false;
		}

		// read handle must not be inherited
		SetHandleInformation( m_hStdErrR, HANDLE_FLAG_INHERIT, 0);
		si.hStdError = m_hStdErrW;
	}

	if (bCaptureStdIn)
	{
		if (! CreatePipe(&m_hStdInR, &m_hStdInW, &saAttr, 0))
		{
			SetError("CreatePipe failed for stdin");
			return false;
		}

		// write handle must not be inherited
		SetHandleInformation( m_hStdInW, HANDLE_FLAG_INHERIT, 0);
		si.hStdInput = m_hStdInR;
	}

	return true;
}
#else
void ClosePipeCheck(int &i)
{
	if (i != -1)
	{
#ifndef NDEBUG
		int iRes =
#endif
		close(i);
		assert(iRes == 0);
		i = -1;
	}
}
#endif // WIN32

template <class _ch> bool MpoProcess::LaunchTemplate(const list <basic_string<_ch> > &cmd_line, bool bCaptureStdOut, bool bCaptureStdIn, bool bCaptureStdErr)
{
	bool bRes = false;

	// make sure all previous state is cleaned up before we try to launch a new process
	Shutdown();

	// windows and unix have different methods for launching a new process
#ifdef WIN32

	BOOL b_result = FALSE;

	// if this is a wide string version
	if (sizeof(_ch) > 1)
	{
		STARTUPINFOW startup_info_wide;
		if (!SetStartupInfoTemplate(startup_info_wide, bCaptureStdOut, bCaptureStdIn, bCaptureStdErr))
		{
			return false;
		}

		// windows wants the command line in one big string
		basic_string<_ch> cmd_s;
		// add command line to one big line win32 string ...
		for (typename list<basic_string<_ch> >::const_iterator li = cmd_line.begin(); li != cmd_line.end(); li++)
		{
			cmd_s += escape_arg(*li);
			cmd_s += ' ';
		}

		size_t stArraySize = cmd_s.size() + 1;	// + 1 for null terminator
		wchar_t *cmd_line_buf = new wchar_t[stArraySize];	// because createprocess won't accept const char *
        SHARED_ARRAY(wchar_t) cleanerupper(cmd_line_buf); // this will free up the memory automatically

		memcpy(cmd_line_buf, cmd_s.c_str(), stArraySize * sizeof(wchar_t));	// this will also copy null terminator over

		b_result = CreateProcessW((LPCWSTR) cmd_line.begin()->c_str(),
			cmd_line_buf, 
			NULL,	// process attributes
			NULL,	// thread attributes
			TRUE,	// inherit handles (must be true for std capturing)

			// The default behavior will be to not create a console window.  This is what VideoStream will expect so this default behavior must be kept.
			// If in the future a console window needs to be created, another optional parameter can be added to address this.
			CREATE_NO_WINDOW,	// creation flags
			NULL,	// new environment
			NULL,	// current directory (same as us)
			&startup_info_wide,	// startup info
			&m_pi);	// process information
	}
	// else if this is the ANSI version
	else
	{
		STARTUPINFO startup_info;
		if (!SetStartupInfoTemplate(startup_info, bCaptureStdOut, bCaptureStdIn, bCaptureStdErr))
		{
			return false;
		}

		// windows wants the command line in one big string
		basic_string<_ch> cmd_s;
		// add command line to one big line win32 string ...
		for (typename list<basic_string<_ch> >::const_iterator li = cmd_line.begin(); li != cmd_line.end(); li++)
		{
			cmd_s += escape_arg(*li);
			cmd_s += ' ';
		}

		size_t stArraySize = cmd_s.size() + 1;	// + 1 for null terminator
		char *cmd_line_buf = new char[stArraySize];	// because createprocess won't accept const char *
        SHARED_ARRAY(char) cleanerupper(cmd_line_buf);	// this will free up the memory automatically

		memcpy(cmd_line_buf, cmd_s.c_str(), stArraySize * sizeof(char));	// this will also copy null terminator over

		b_result = CreateProcessA((LPCSTR) cmd_line.begin()->c_str(),
			cmd_line_buf, 
			NULL,	// process attributes
			NULL,	// thread attributes
			TRUE,	// inherit handles (must be true for std capturing)

			// The default behavior will be to not create a console window.  This is what VideoStream will expect so this default behavior must be kept.
			// If in the future a console window needs to be created, another optional parameter can be added to address this.
			CREATE_NO_WINDOW,	// creation flags
			NULL,	// new environment
			NULL,	// current directory (same as us)
			&startup_info,	// startup info
			&m_pi);	// process information
	}

	// We will never use these handles (they've been passed to the child process),
	//  so for the sake of organization, we should close them immediately here.
	// Otherwise we have to close them in Shutdown() and that is far more confusing.
	CloseHandleCheck(m_hStdErrW);
	CloseHandleCheck(m_hStdOutW);
	CloseHandleCheck(m_hStdInR);

	if (b_result != 0)
	{
		bRes = true;
		m_bProcessRunning = true;
	}
	else
	{
		string s = "CreateProcess failed with code: " + numstr::ToStr(GetLastError());
		SetError(s);
	}

#else
		if (bCaptureStdOut)
		{
			if (pipe(m_iStdOut) != 0)
			{
				SetError("pipe failed for stdout");
				return false;
			}
		}

		if (bCaptureStdErr)
		{
			if (pipe(m_iStdErr) != 0)
			{
				SetError("pipe failed for stderr");
				return false;
			}
		}

		if (bCaptureStdIn)
		{
			if (pipe(m_iStdIn) != 0)
			{
				SetError("pipe failed for stdin");
				return false;
			}
		}

	const char **argv = (const char **) malloc(sizeof (const char *) * (cmd_line.size() + 1));
	// allocate memory to store command line, add an extra slot for NULL termination

	unsigned int i = 0;

	for (list<string>::const_iterator li = cmd_line.begin(); li != cmd_line.end(); li++)
	{
		argv[i] = li->c_str();
		++i;
	}
	argv[i] = NULL;	// final null terminating entry

	// prepare to execute child process
	m_pid = fork();

	// if we are the child process
	if (m_pid == 0)
	{
		if (bCaptureStdOut)
		{
			close(m_iStdOut[0]);
			if (dup2(m_iStdOut[1], STDOUT_FILENO) == -1) perror("dup2 stdout failed");
		}
		if (bCaptureStdErr)
		{
			close(m_iStdErr[0]);
			if (dup2(m_iStdErr[1], STDERR_FILENO) == -1) perror("dup2 stderr failed");
		}
		if (bCaptureStdIn)
		{
			close(m_iStdIn[1]);
			if (dup2(m_iStdIn[0], STDIN_FILENO) == -1) perror("dup2 stdin failed");
		}

		// NOTE : we want this to be execvp so it uses the PATH to search for executables
		execvp(argv[0], (char* const*) argv);

		// if execvp failed, it will continue executing this function
		_exit(127);	// critical shutdown, return a result code to help us determine whether app ever ran
	}

	// else we are the parent process
	else
	{
		// pipe (7) man page recommends this
		ClosePipeCheck(m_iStdOut[1]);
		ClosePipeCheck(m_iStdErr[1]);
		ClosePipeCheck(m_iStdIn[0]);

		// The only way (that I know of) to detect whether a child prcoess failed to launch is to call waitpid and check the exit code (it will be 127 if it didn't launch).
		// However, we have no way of knowing (I haven't found a way) to know when the child thread has had a chance to call execvp.
		// So this kinda sucks, we just have to wait an indefinite amount of time and then call waitpid.
		// So as a solution, I'm always going to return true here and let the caller call Wait to see if there was an error.
		bRes = true;
		m_bProcessRunning = true;
		
		// Q: How about waiting a little while to "guarantee" that the other process is done?
		// A: This would hurt performance so it's unacceptable.
	}	

	free(argv);
#endif

	if (m_bProcessRunning)
	{
		// *** IMPORTANT! ***
		// This variable MUST be set to true before any threads are started (or rather, before m_bParentAckChildren is set to true).
		// If not, there is a random chance that any given thread will see this bool set to false and will exit prematurely.
		m_bThreadsShouldBeRunning = true;

		// so child threads know when parent has acknowledged them
		// (to prevent case of Shutdown being called immediately after Launch)
		m_bParentAckChildren = false;

		// launch listener threads
		if (bCaptureStdOut)
		{
			mpo_create_thread(&m_threadOut, StdOutThread, this);
			m_bStdOutNeedsJoin = true;	// need to wait for this thread later to clean up resources
			while (!m_bStdOutRunning) MpoTimerUtil::MakeDelay(1);	// wait for thread to come online
		}

		if (bCaptureStdErr)
		{
			mpo_create_thread(&m_threadErr, StdErrThread, this);
			m_bStdErrNeedsJoin = true;	// need to wait for this thread later to clean up resources
			while (!m_bStdErrRunning) MpoTimerUtil::MakeDelay(1);	// wait for thread to come online
		}

		// Signal to child threads that we know they're running.
		// This is to make sure our state is known so that we don't get into any endless loops during thread I/O.
		m_bParentAckChildren = true;		
	}

	return bRes;
}

bool MpoProcess::Launch(const list <wstring> &cmd_line, bool bCaptureStdOut, bool bCaptureStdIn, bool bCaptureStdErr)
{
#ifdef WIN32
	return LaunchTemplate(cmd_line, bCaptureStdOut, bCaptureStdIn, bCaptureStdErr);
#else
	// There is no wide command-line for unix, so we need to down-convert it using UTF8 (which seems to be a prevailing standard)
	// When in doubt, do not call this version of Launch from unix!
	list<string> lstCmdLine8;

	for (list<wstring>::const_iterator li = cmd_line.begin(); li != cmd_line.end(); li++)
	{
		string strUTF8;
		if (mpom::ToUTF8(strUTF8, *li))
		{
			lstCmdLine8.push_back(strUTF8);
		}
		else
		{
			this->SetError("UTF8 conversion failed.");
			return false;
		}
	}

	return LaunchTemplate(lstCmdLine8, bCaptureStdOut, bCaptureStdIn, bCaptureStdErr);
#endif
}

bool MpoProcess::Launch(const list <string> &cmd_line, bool bCaptureStdOut, bool bCaptureStdIn, bool bCaptureStdErr)
{
	return LaunchTemplate(cmd_line, bCaptureStdOut, bCaptureStdIn, bCaptureStdErr);
}

MpoProcess::waitr MpoProcess::Wait(int *exit_code, unsigned int uTimeoutMs)
{
	waitr res = WAITR_BUSY;

	if (!m_bProcessRunning)
	{
		return WAITR_ERROR;
	}

#ifdef WIN32
	DWORD wait_result = WaitForSingleObject(m_pi.hProcess, uTimeoutMs);

	// if process has ended
	if (wait_result == WAIT_OBJECT_0)
	{
		res = WAITR_FINISHED;
		m_bProcessRunning = false;

		if (exit_code != NULL)
		{
			DWORD tmp = 0;
			GetExitCodeProcess(m_pi.hProcess, &tmp);
			*exit_code = (int) tmp;
		}

		CloseHandle(m_pi.hProcess);
	}
	else if (wait_result != WAIT_TIMEOUT)
	{
		res = WAITR_ERROR;
	}
	// else BUSY (TODO handle errors too!)
#else
	unsigned int uStartTime = refresh_timer();
	int status = 0;

	// this needs to be a 'do/while' to ensure the loop goes through at least one time
	do
	{
		pid_t p = waitpid(m_pid, &status, WNOHANG);	// check to see if child has exited, but don't wait
		
		// some error
		if (p == -1)
		{
			res = WAITR_ERROR;
			break;
		}
		// else if the child we were waiting for is done, we're done too
		else if (p == m_pid)
		{
			res = WAITR_FINISHED;
			int iExitCode = 0;
			// if child exited properly (without segfaulting, for example)
			if (WIFEXITED(status) == true)
			{
				iExitCode = WEXITSTATUS(status);
			}
			else iExitCode = -1;	// this will be our generic error for improper termination

			// if we get this exit code, it means our child process never launched (exec failed)
			if (iExitCode == 127)
			{
				res = WAITR_NEVER_LAUNCHED;
			}
			
			if (exit_code != NULL)
			{
				*exit_code = iExitCode;
			}
						
			m_bProcessRunning = false;
			break;
		}
		// else p is either 0 ('timed out') or an unknown
		//  so we treat it as a timeout.
		else
		{
			make_delay(1);	// don't hog CPU
		}
	} while (get_elapsed_ms(uStartTime) < uTimeoutMs);
#endif // WIN32

	return res;
}

string MpoProcess::ReadStdOutEx(bool bFinal)
{
	// if caller wants all the remaining data
	if (bFinal)
	{
		// process must no be running or else this could lead to deadlocks
		if (m_bProcessRunning)
		{
			throw runtime_error("ReadStdOut with bFinal true was called while process was still running.");
		}

		// If thread hasn't exited formally (by calling wait thread), do so now.
		// This should happen pretty soon because the pipe from the child process will be closed.
		// TODO : change this to a timeout in the future in case something goes wrong
		if (m_bStdOutNeedsJoin)
		{
			mpo_wait_thread(&m_threadOut);
			m_bStdOutNeedsJoin = false;
		}
	}

	string strRes;
	m_pMutex->Lock();
	strRes = m_strStdOut;
	m_strStdOut.clear();
	m_pMutex->Unlock();
	return strRes;
}

string MpoProcess::ReadStdErrEx(bool bFinal)
{
	// if caller wants all the remaining data
	if (bFinal)
	{
		// process must no be running or else this could lead to deadlocks
		if (m_bProcessRunning)
		{
			throw runtime_error("ReadStdErr with bFinal true was called while process was still running.");
		}

		// If thread hasn't exited formally (by calling wait thread), do so now.
		// This should happen pretty soon because the pipe from the child process will be closed.
		// TODO : change this to a timeout in the future in case something goes wrong
		if (m_bStdErrNeedsJoin)
		{
			mpo_wait_thread(&m_threadErr);
			m_bStdErrNeedsJoin = false;
		}
	}

	string strRes;
	m_pMutex->Lock();
	strRes = m_strStdErr;
	m_strStdErr.clear();
	m_pMutex->Unlock();
	return strRes;
}

void MpoProcess::WriteStdInEx(const void *pBuf, size_t stBufSizeBytes)
{
	// If process isn't running, then the stdin pipe may be alive with no one listening at the other end, so don't let the user get blocked.
	if (!m_bProcessRunning)
	{
		throw runtime_error("Process isn't running.");
	}

	// check to see if pipe is closed
#ifdef WIN32
	if (m_hStdInW == (HANDLE) -1)
#else
	if (m_iStdIn[1] == -1)
#endif
	{
		throw runtime_error("StdIn pipe is closed.");
	}

	bool bRemotePipeClosed = false;
	string strErrMsg;
	size_t stBytesToCopy = stBufSizeBytes;
	const char *p8Buf = (const char *) pBuf;
	const char *p8BufEnd = p8Buf + stBytesToCopy;

	// while we haven't emptied the buffer (if the buffer is empty, this does nothing)
	while (p8Buf != p8BufEnd)
	{
#ifdef WIN32
		DWORD dwBytesWritten = 0;
		BOOL b = WriteFile(m_hStdInW, p8Buf, stBytesToCopy, &dwBytesWritten, NULL);
		if (b)
		{
			// prepare to send the remainder of the buf
			p8Buf += dwBytesWritten;
			stBytesToCopy -= dwBytesWritten;
		}

		// If WriteFile fails, it means that the remote process has closed the pipe
		//  (probably by exiting).

		// else if we don't know that process has exited
		else if (m_bProcessRunning)
		{
			strErrMsg = "WriteFile got error " + numstr::ToStr(GetLastError());
			bRemotePipeClosed = true;
			break;
		}
		// else process has exited, so we probably got a broken pipe error
		else
		{
			bRemotePipeClosed = true;
			break;
		}
#else
	// UNIX
//	printf("stdinthread about to call write\n");
		ssize_t iRes = write(m_iStdIn[1], p8Buf, stBytesToCopy);
		if (iRes > 0)
		{
			p8Buf += iRes;
			stBytesToCopy -= iRes;
		}
		else if (iRes < 0)
		{
			strErrMsg = "write got error " + numstr::ToStr((int) iRes);
			bRemotePipeClosed = true;
			break;
		}
		// else EOF
		else
		{
			bRemotePipeClosed = true;
			break;
		}
#endif // WIN32
	} // end while we haven't sent all the buffer

	// if the remote pipe is closed, close our local pipes so that we don't keep trying to write to the closed pipes over and over again
	if (bRemotePipeClosed)
	{
#ifdef WIN32
		CloseHandleCheck(m_hStdInW);
#else
		ClosePipeCheck(m_iStdIn[1]);
#endif
	}

	// If we have an error message we'd like the caller to know about, pass it up to them
	if (!strErrMsg.empty())
	{
		throw runtime_error(strErrMsg);
	}

}

void MpoProcess::CloseStdIOThreads()
{
	m_bThreadsShouldBeRunning = false;

	// Close stdin pipe to child process so that child process stops waiting for input from stdin (if it was).
	// This allows the child process a chance to gracefully shutdown.
#ifdef WIN32
	CloseHandleCheck(m_hStdInW);
#else
	ClosePipeCheck(m_iStdIn[1]);
#endif

}

MpoProcess::MpoProcess() :
m_bProcessRunning(false),
m_bThreadsShouldBeRunning(false),
m_bParentAckChildren(false),
m_bEOF(false),
m_bStdOutRunning(false),
m_bStdErrRunning(false),
m_bStdOutNeedsJoin(false),
m_bStdErrNeedsJoin(false),
m_threadOut(0),
m_threadErr(0)
#ifdef WIN32
,m_hStdErrR((HANDLE) -1),
m_hStdErrW((HANDLE) -1),
m_hStdOutR((HANDLE) -1),
m_hStdOutW((HANDLE) -1),
m_hStdInR((HANDLE) -1),
m_hStdInW((HANDLE) -1)
#endif // WIN32
{
#ifndef WIN32
	for (int i = 0; i < 2; i++)
	{
		m_iStdOut[i] = -1;
		m_iStdErr[i] = -1;
		m_iStdIn[i] = -1;
	}
#endif
}

MpoProcess::~MpoProcess()
{
	Shutdown();
}

void MpoProcess::DeleteInstance()
{
	delete this;
}

bool MpoProcess::RequestShutdown(bool bNice)
{
	bool bRes = false;

	// if this is a nice shutdown request, then wait a little while for child to exit
	if (bNice)
	{
#ifndef WIN32
		// send a signal and wait
		bRes = (kill(m_pid, SIGINT) == 0);
#else
		assert(false);	// no support for this on windows
#endif
	}
	// else a forceful shutdown request
	else
	{
		bRes = Kill();
		m_bProcessRunning = false;
	}

	return bRes;
}

bool MpoProcess::Kill()
{
	bool bSuccess = false;

#ifdef WIN32
	// forcefully kill the process as request from caller
	BOOL b = TerminateProcess(m_pi.hProcess, -1);
	DWORD dwRes = GetLastError();
	bSuccess = (b != 0);
#else
	if (kill(m_pid, SIGKILL) == 0)
	{
		bSuccess = true;
	}
	else
	{
		int i = errno;

#ifndef __APPLE__
		char buf[80];
		printf("kill request failed: %s\n", strerror_r(i, buf, sizeof(buf)));
#else
		printf("kill request failed: %u\n", i);
#endif
	}
#endif // WIN32
	return bSuccess;
}

void MpoProcess::Shutdown()
{
	// close any open stdio threads since we are saying goodbye
	CloseStdIOThreads();

	// Wait for the stdout/stderr listener threads to exit.
	// This is necessary to clean up resources used by the threads.
	// TODO : change this to wait for a timeout period and then forcefully kill the threads.
	if (m_bStdOutNeedsJoin)
	{
		mpo_wait_thread(&m_threadOut);
		m_bStdOutNeedsJoin = false;
		assert(!m_bStdOutRunning);
	}
	if (m_bStdErrNeedsJoin) 
	{
		mpo_wait_thread(&m_threadErr);
		m_bStdErrNeedsJoin = false;
		assert(!m_bStdErrRunning);
	}

	// close local handles/pipes as part of the clean-up process
	// NOTE : stdin handle has already been closed by CloseStdIOThreads call
#ifdef WIN32
	CloseHandleCheck(m_hStdErrR);
	CloseHandleCheck(m_hStdOutR);
#else
	ClosePipeCheck(m_iStdErr[0]);
	ClosePipeCheck(m_iStdOut[0]);
#endif
}

void MpoProcess::SetError(const string &strLastErr)
{
	// this may be called from a thread
	m_pMutex->Lock();
	m_strLastErr = strLastErr;
	m_pMutex->Unlock();
}

/////////////////////////////////////////////////////////////////

// Maximum size that the stdout/stderr buffers can be before these threads stop reading from the child process's pipe.
// (this is to prevent the buffers getting too big and the app crashing due to running out of memory)
#define MPO_PROCESS_MAX_BUFFER_SIZE_BYTES (1024 * 1024 * 10)

// helper function that adds to and existing buffer and will wait if the buffer is too big
void MpoProcess::AddToBuf(MpoProcess *pProc, string *pStrDstBuf, const void *pSrcBuf, size_t stSrcBufSizeBytes)
{
	size_t stBufSize = 0;

	// need to mutex-guard string because it is accessed by multiple threads
	pProc->m_pMutex->Lock();
	pStrDstBuf->append(string((const char *) pSrcBuf, stSrcBufSizeBytes));
	stBufSize = pStrDstBuf->size();
	pProc->m_pMutex->Unlock();

	// if buffer is too big, then we need to wait until the caller calls ReadStdOut/ReadStdErr/etc to reduce the buffer size.
	// (but we don't want to wait if the parent process is telling us to shutdown anyway)
	while ((stBufSize > MPO_PROCESS_MAX_BUFFER_SIZE_BYTES) && (pProc->m_bThreadsShouldBeRunning))
	{
		MpoTimerUtil::MakeDelay(250);	// no point hogging cpu, and also we don't want to waste cycles by locking/unlocking the mutex excessively
		pProc->m_pMutex->Lock();
		stBufSize = pStrDstBuf->size();
		pProc->m_pMutex->Unlock();
	}
}

void *MpoProcess::StdOutThread(void *pInstance)
{
	MpoProcess *pProc = (MpoProcess *) pInstance;

	// acknowledge that we're running
	pProc->m_bStdOutRunning = true;
//	printf("StdOutThread: m_bStdOutRunning set to true\n");	// remove

	// wait for parent thread to know that we're running
	while (!pProc->m_bParentAckChildren) MpoTimerUtil::MakeDelay(1);

	// do work
	while (pProc->m_bThreadsShouldBeRunning)
	{
		// buf is bigger to handle large chunks of stream data
#ifdef DEBUG
		// make it really small to test the case of the buffer not being big enough to receive all of the data
		char buf[2];
#else
		char buf[16384];
#endif // DEBUG
#ifdef WIN32
		DWORD dwBytesRead = 0;
		BOOL b = ReadFile(pProc->m_hStdOutR, buf, sizeof(buf), &dwBytesRead, NULL);
		if (b)
		{
			AddToBuf(pProc, &pProc->m_strStdOut, buf, dwBytesRead);
		}
		// else if process is still running, this is an error
		else if (pProc->m_bProcessRunning)
		{
			// GetLastError is thread safe
			string s = "StdOutThread ReadFile got error " + numstr::ToStr(GetLastError());

			// SetError is thread safe
			pProc->SetError(s);
			
			break;
		}
		// else process has exited, so we probably got a broken pipe error
		else
		{
			break;
		}
#else
		//UNIX
//printf("stdout about to call read\n");

		ssize_t iBytesRead = read(pProc->m_iStdOut[0], buf, sizeof(buf));
		if (iBytesRead > 0)
		{
//			printf("stdout Read got something: %s\n", string(buf, iBytesRead).c_str());
			// need to mutex-guard string because it is accessed by multiple threads
			AddToBuf(pProc, &pProc->m_strStdOut, buf, (size_t) iBytesRead);
		}
		else if (iBytesRead < 0)
		{
//			printf("stdout read error\n");
			string s = "StdOutThread ReadFile got error " + numstr::ToStr((int) iBytesRead);
			pProc->SetError(s);
			break;
		}
		// else EOF
		else
		{
//			printf("stdout eof\n");
			break;
		}
#endif // WIN32
	}

	// acknowledge that we're no longer running
	pProc->m_bStdOutRunning = false;
//	printf("StdOutThread: m_bStdOutRunning set to false\n");	// remove

	return NULL;
}

void *MpoProcess::StdErrThread(void *pInstance)
{
	MpoProcess *pProc = (MpoProcess *) pInstance;
//	bool bRemotePipeClosed = false;	// whether process has stopped sending stderr

	// acknowledge that we're running
	pProc->m_bStdErrRunning = true;
//printf("StdErrThread: m_bStdErrRunning set to true\n");	// remove

	// wait for parent thread to know that we're running
	while (!pProc->m_bParentAckChildren) MpoTimerUtil::MakeDelay(1);

	// do work
	while (pProc->m_bThreadsShouldBeRunning)
	{
		// buf is bigger to handle large chunks of stream data
		char buf[16384];
#ifdef WIN32
		DWORD dwBytesRead = 0;
		BOOL b = ReadFile(pProc->m_hStdErrR, buf, sizeof(buf), &dwBytesRead, NULL);
		if (b)
		{
			AddToBuf(pProc, &pProc->m_strStdErr, buf, dwBytesRead);
		}
		// else if process is still running, this is an error
		else if (pProc->m_bProcessRunning)
		{
			// GetLastError is thread safe
			string s = "StdErrThread ReadFile got error " + numstr::ToStr(GetLastError());

			// SetError is thread safe
			pProc->SetError(s);
			break;
		}
		// else process has exited, so we probably got a broken pipe error
		else
		{
			break;
		}
#else
		//UNIX
//		printf("sterr about to call read\n");
		ssize_t iBytesRead = read(pProc->m_iStdErr[0], buf, sizeof(buf));
		if (iBytesRead > 0)
		{
//			printf("stderr read success : %s\n", string(buf, iBytesRead).c_str());
			// need to mutex-guard string because it is accessed by multiple threads
			AddToBuf(pProc, &pProc->m_strStdErr, buf, (size_t) iBytesRead);
		}
		else if (iBytesRead < 0)
		{
//			printf("stderr read error\n");
			string s = "StdErrThread ReadFile got error " + numstr::ToStr((int) iBytesRead);
			pProc->SetError(s);
			break;
		}
		// else EOF
		else
		{
//			printf("stderr EOF from pipe/read call\n");
			break;
		}
#endif // WIN32
	}

	// acknowledge that we're no longer running
	pProc->m_bStdErrRunning = false;
//	printf("StdErrThread: m_bStdErrRunning set to false\n");	// remove

	return NULL;
}
