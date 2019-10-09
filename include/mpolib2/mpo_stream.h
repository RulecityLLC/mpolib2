#ifndef MPO_STREAM_H
#define MPO_STREAM_H

#include "mpo_dll.h"
#include "mpo_types.h"
#include "mpo_deleter.h"

#include <string>

using namespace std;

typedef enum
{
	MSG_OK,	// success (partial or full data was transferred)
	MSG_ERROR,	// error (data was partially transferred or not transferred at all)
	MSG_TIMEOUT,	// timeout occurred before ANY data was transferred
	MSG_END,	// end of the stream, NO DATA WAS TRANSFERRED, convenience for higher level protocols
} StreamMsg;

class INonblockingStream
{
public:
	virtual size_t Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs) = 0;

	// returns true if calling Read would put at least 1 byte into the destination buffer
	virtual bool IsReadByteWaiting() = 0;

	virtual size_t Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs) = 0;

	virtual StreamMsg GetLastMsg() = 0;
};

typedef shared_ptr<INonblockingStream> nonblocking_sharedptr;

// A dummy class that accepts a buffer when created and then spits that buffer back out.
// Cannot be written to, can only be read.
// Used to test things like HTTP decoding (without having a real HTTP server).
class EXPORT_ME NonblockingStreamTester : public INonblockingStream, public MpoDeleter
{
public:
	// put the buffer here that you want spit back out
	static nonblocking_sharedptr GetInstance(const void *buf, size_t bufsize);

	size_t Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs);

	bool IsReadByteWaiting();

	// write doesn't work at all
	size_t Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs);

	StreamMsg GetLastMsg();
private:
	NonblockingStreamTester();

	void DeleteInstance();

	// the beginning of the buffer
	const unsigned char *m_p8Buf;

	// total size of buffer
	size_t m_stBufSize;

	// current position in buffer
	size_t m_stBufIdx;
};

/////////////////////////
//typedef const struct vldp_out_info *(*initproc)(const struct vldp_in_info *in_info);

typedef size_t (*ReadCallback)(void *, size_t, unsigned int);
typedef size_t (*WriteCallback)(const void *buf, size_t, unsigned int);
typedef StreamMsg (*GetLastMsgCallback)();

// A test class that lets you define your own callbacks for Read/Write and GetLastMsg
class EXPORT_ME NonblockingStreamCallbacks : public INonblockingStream, public MpoDeleter
{
public:
	// put the buffer here that you want spit back out
	static nonblocking_sharedptr GetInstance(ReadCallback pOnRead,
		WriteCallback pOnWrite,
		GetLastMsgCallback pOnGetLastMsg);

	size_t Read(void *buf, size_t stBytesToRead, unsigned int uTimeoutMs);

	bool IsReadByteWaiting();

	size_t Write(const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs);

	StreamMsg GetLastMsg();
private:
	NonblockingStreamCallbacks(ReadCallback pOnRead,
		WriteCallback pOnWrite,
		GetLastMsgCallback pOnGetLastMsg);
	void DeleteInstance();

	ReadCallback m_pOnRead;
	WriteCallback m_pOnWrite;
	GetLastMsgCallback m_pOnGetLastMsg;
};

/////////////////////////

// helper functions which will 'convert' nonblocking calls into semi-blocking calls.
// These functions only return when the full number of bytes are processed or timeout occurs.
class EXPORT_ME StreamFull
{
public:
	static StreamMsg Read(INonblockingStream *pStream, void *buf, size_t stBytesToRead, unsigned int uTimeoutMs);
	static StreamMsg Read(INonblockingStream *pStream, void *buf, size_t stBytesToRead, size_t &stBytesRead, unsigned int uTimeoutMs);
	static StreamMsg Write(INonblockingStream *pStream, const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs);

	// writes the buffer and adds a CRLF to the end (used by HTTP, and SMTP)
	static StreamMsg WriteCRLF(INonblockingStream *pStream, const void *buf, size_t stBytesToWrite, unsigned int uTimeoutMs);

	// Read until a terminating CRLF is reached.
	// The CRLF is not added to the resulting buffer.
	static StreamMsg ReadUntilCRLF(INonblockingStream *pStream, string &buf, unsigned int uTimeoutMs);
};

class IBlockingStream
{
public:
	virtual size_t Read(void *buf, size_t stBytesToRead) = 0;

	virtual size_t Write(const void *buf, size_t stBytesToWrite) = 0;

	virtual bool Seek(MPO_INT64 i64Offset, seek_type origin) = 0;

	// returns length of stream
	virtual MPO_UINT64 GetLength() = 0;

	// returns current position within stream
	virtual MPO_UINT64 GetPosition() = 0;

	virtual bool CanRead() = 0;

	virtual bool CanWrite() = 0;

	virtual bool CanSeek() = 0;

};

typedef shared_ptr<IBlockingStream> blocking_sharedptr;

class EXPORT_ME StreamHelpers
{
public:
	static string StreamToString(IBlockingStream *pStream);
};

#endif // MPO_STREAM_H
