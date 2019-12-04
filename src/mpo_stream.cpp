#include <mpolib/mpo_stream.h>
#include <mpolib/mpo_timer.h>
#include <string.h>	// for memcpy

nonblocking_sharedptr NonblockingStreamTester::GetInstance(const void *buf, size_t bufsize)
{
	NonblockingStreamTester *pInstance = new NonblockingStreamTester();
	pInstance->m_p8Buf = (const unsigned char *) buf;
	pInstance->m_stBufSize = bufsize;
	return shared_ptr<NonblockingStreamTester>(pInstance, NonblockingStreamTester::deleter());
}

void NonblockingStreamTester::DeleteInstance()
{
	delete this;
}

////

size_t NonblockingStreamTester::Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs)
{
	size_t stRes = stBytesToRead;

	size_t stBytesLeft = m_stBufSize - m_stBufIdx;
	if (stBytesToRead > stBytesLeft)
	{
		stRes = stBytesLeft;
	}

	if (stRes > 0)
	{
		memcpy(buf, m_p8Buf + m_stBufIdx, stRes);
		m_stBufIdx += stRes;
	}

	return stRes;
}

bool NonblockingStreamTester::IsReadByteWaiting()
{
	return (m_stBufSize > m_stBufIdx);
}

size_t NonblockingStreamTester::Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs)
{
	return 0;
}

StreamMsg NonblockingStreamTester::GetLastMsg()
{
	StreamMsg msg = MSG_END;

	if (m_stBufIdx < m_stBufSize)
	{
		msg = MSG_OK;
	}

	return msg;
}

NonblockingStreamTester::NonblockingStreamTester() :
m_stBufIdx(0)
{
}

///////////////////////////////

nonblocking_sharedptr NonblockingStreamCallbacks::GetInstance(ReadCallback pOnRead,
		WriteCallback pOnWrite,
		GetLastMsgCallback pOnGetLastMsg)
{
	return nonblocking_sharedptr(new NonblockingStreamCallbacks(pOnRead,
		pOnWrite, pOnGetLastMsg), NonblockingStreamCallbacks::deleter());
}

size_t NonblockingStreamCallbacks::Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs)
{
	return m_pOnRead(buf, stBytesToRead, uTimeoutMs);
}

bool NonblockingStreamCallbacks::IsReadByteWaiting()
{
	return true;	// this isn't necessarily correct, it can be fixed later if it becomes a problem
}

size_t NonblockingStreamCallbacks::Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs)
{
	return m_pOnWrite(buf, stBytesToWrite, uTimeoutMs);
}

StreamMsg NonblockingStreamCallbacks::GetLastMsg()
{
	return m_pOnGetLastMsg();
}

NonblockingStreamCallbacks::NonblockingStreamCallbacks(ReadCallback pOnRead,
		WriteCallback pOnWrite,
		GetLastMsgCallback pOnGetLastMsg) :
m_pOnRead(pOnRead),
m_pOnWrite(pOnWrite),
m_pOnGetLastMsg(pOnGetLastMsg)
{
}

void NonblockingStreamCallbacks::DeleteInstance()
{
	delete this;
}

///////////////////////////////

StreamMsg StreamFull::Read(INonblockingStream *pStream, void *buf, size_t stBytesToRead, unsigned int uTimeoutMs)
{
	size_t stBytesRead = 0;

	StreamMsg res = Read(pStream, buf, stBytesToRead, stBytesRead, uTimeoutMs);

	// caller expects the bytes read to be equal to the bytes to read
	if (stBytesRead != stBytesToRead)
	{
		res = MSG_ERROR;
	}

	return res;
}

StreamMsg StreamFull::Read(INonblockingStream *pStream, void *buf, size_t stBytesToRead, size_t &stBytesRead, unsigned int uTimeoutMs)
{
	StreamMsg res = MSG_TIMEOUT;
	size_t stCurBytesRead = 0;
	size_t stBytesLeft = stBytesToRead;
	unsigned char *pu8Buf = (unsigned char *) buf;

	stBytesRead = 0;
	unsigned int uMsTimer = MpoTimerUtil::RefreshTimer();

	// go until we timeout (or fill our goal)
	while ((MpoTimerUtil::RefreshTimer() - uMsTimer) < uTimeoutMs)
	{
		stCurBytesRead = pStream->Read(pu8Buf, stBytesLeft, uTimeoutMs);
		stBytesRead += stCurBytesRead;
		stBytesLeft -= stCurBytesRead;
		pu8Buf += stCurBytesRead;

		// if we finished sending everything, then we're done
		if (stBytesLeft == 0)
		{
			res = MSG_OK;
			break;
		}
		// else we wrote less than we requested, so find out why
		else
		{
			StreamMsg msg = pStream->GetLastMsg();

			// if we didn't write anything or timeout, then something went wrong, so this is an error
			if ((msg != MSG_OK) && (msg != MSG_TIMEOUT))
			{
				// if we read something, it could mean we received data and peer disconnected, which is valid as long
				//  as we got the number of bytes we were expecting to get.
				if (stBytesRead > 0)
				{
					res = MSG_OK;
				}
				// else if we got nothing, it probably means the peer was already disconnected
				else
				{
					res = msg;
				}
				break;
			}
			// else keep waiting to write
		}
		MpoTimerUtil::MakeDelay(1);
	}

	// If we got something before timing out, then we do not consider it a timeout.
	// A timeout implies no data was received at all.
	if ((res == MSG_TIMEOUT) && (stBytesRead > 0))
	{
		res = MSG_OK;
	}

	return res;
}

StreamMsg StreamFull::Write(INonblockingStream *pStream, const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs)
{
	StreamMsg res = MSG_TIMEOUT;
	size_t stCurBytesWritten = 0;
	size_t stBytesLeft = stBytesToWrite;
	const unsigned char *pu8Buf = (const unsigned char *) buf;

	unsigned int uMsTimer = MpoTimerUtil::RefreshTimer();

	// go until we timeout (or fill our goal)
	while ((MpoTimerUtil::RefreshTimer() - uMsTimer) < uTimeoutMs)
	{
		stCurBytesWritten = pStream->Write(pu8Buf, stBytesLeft, uTimeoutMs);
		stBytesLeft -= stCurBytesWritten;
		pu8Buf += stCurBytesWritten;

		// if we finished sending everything, then we're done
		if (stBytesLeft == 0)
		{
			res = MSG_OK;
			break;
		}
		// else we wrote less than we requested, so find out why
		else
		{
			StreamMsg msg = pStream->GetLastMsg();

			// if we didn't write anything or timeout, then something went wrong, so this is an error
			if ((msg != MSG_OK) && (msg != MSG_TIMEOUT))
			{
				res = msg;
				break;
			}
			// else keep waiting to write
		}
		MpoTimerUtil::MakeDelay(1);
	}

	return res;
}

StreamMsg StreamFull::WriteCRLF(INonblockingStream *pStream, const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs)
{
	StreamMsg res = Write(pStream, buf, stBytesToWrite, uTimeoutMs);
	if (res == MSG_OK)
	{
		const char CRLF[2] = { 13, 10 };
		res = Write(pStream, CRLF, sizeof(CRLF), uTimeoutMs);
	}

	return res;
}

StreamMsg StreamFull::ReadUntilCRLF(INonblockingStream *pStream, string &buf, unsigned int uTimeoutMs)
{
	bool got_cr = false;	// if we've encountered a carriage return
	StreamMsg msg = MSG_ERROR;
//	size_t stBytesRead = 0;
	char ch = 0;

	buf = "";

	// go until break
	for (;;)
	{
		/*stBytesRead = */pStream->Read(&ch, 1, uTimeoutMs);
		// 1 character at a time.. easy to ensure accuracy, because we need not maintain
		// overflow buffers, but not as efficient.  We can optimize it later.

		msg = pStream->GetLastMsg();

		buf += ch;	// add character we've read to our buffer

		if (msg == MSG_OK)
		{
			// if we haven't got a CR yet
			if (!got_cr)
			{
				if (ch == 13) got_cr = true;
				// else it's not a CR, so do nothing

				// premature line feed (openssl's test client does this)
				// (and so do some LAME web servers)
				if (ch == 10)
				{
					buf.erase(buf.size()-1,1);	// get rid of LF we just added
					break;
				}

			}
			// else if we got a LF after having got a CR, then our line is done,
			// and our buffer contains at least 2 characters
			else if (ch == 10)
			{
				size_t size = buf.size();

#ifdef DEBUG
				assert(size >= 2);
#endif

				// this safety check is really kind of redundant but it's better than segfaulting
				if (size >= 2) buf.erase(size-2, 2);	// erase trailing CRLF if they exist
				// else segfault? :)
				break;
			}
			// else if we didn't get another CR, and it's not a linefeed, then reset our state
			else if (ch != 13) got_cr = false;
			// else it's anoter CR so change nothing
		}
		// else something went wrong, so we should abort the loop and return the error result
		else
		{
			break;
		}
	} // end loop

	return msg;
}

string StreamHelpers::StreamToString(IBlockingStream *pStream)
{
	string strRes;
	unsigned char buf[4096];	// arbitrary size

	size_t stRes = 0;

	for (;;)
	{
		stRes = pStream->Read(buf, sizeof(buf));
		strRes += string((char *) buf, stRes);
		if (stRes != sizeof(buf))
		{
			break;
		}
	}

	return strRes;
}
