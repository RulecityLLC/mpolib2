#include <mpolib/mpo_file_stream.h>
#include <mpolib/mpo_misc.h>
#include "mpo_file_stream_internal.h"
#include "mpo_fileio_internal.h"
#include <stdexcept>

blocking_sharedptr MpoFileStreamFactory::CreateInstance(const char *filename, open_type flags)
{
    blocking_sharedptr pRes = MpoFileStream::GetInstance(filename, flags);
    if (pRes.get() == nullptr)
    {
        string msg = "File ";
        msg += filename;
        msg += " could not be opened";
        throw runtime_error(msg);
    }
    return pRes;
}

blocking_sharedptr MpoFileStreamFactory::GetInstance(const char *filename, open_type flags)
{
	return MpoFileStream::GetInstance(filename, flags);
}

blocking_sharedptr MpoFileStreamFactory::CreateInstance(const wstring &filename, open_type flags)
{
    blocking_sharedptr pRes = MpoFileStream::GetInstance(filename, flags);
    if (pRes.get() == nullptr)
    {
        string msg = "File ";
        msg += mpom::str_conv(filename);
        msg += " could not be opened";

        throw runtime_error(msg);
    }
    return pRes;
}

blocking_sharedptr MpoFileStreamFactory::GetInstance(const wstring &filename, open_type flags)
{
	return MpoFileStream::GetInstance(filename, flags);
}

IMpoFileStreamSPtr MpoFileStreamFactory::CreateInstanceFileStream(const char* filename, open_type flags)
{
	IMpoFileStreamSPtr pRes = MpoFileStream::GetInstanceFileStream(filename, flags);
	if (pRes.get() == nullptr)
	{
		string msg = "File ";
		msg += filename;
		msg += " could not be opened";

		throw runtime_error(msg);
	}
	return pRes;
}

IMpoFileStreamSPtr MpoFileStreamFactory::CreateInstanceFileStream(const wstring& filename, open_type flags)
{
	IMpoFileStreamSPtr pRes = MpoFileStream::GetInstanceFileStream(filename, flags);
	if (pRes.get() == nullptr)
	{
		string msg = "File ";
		msg += mpom::str_conv(filename);
		msg += " could not be opened";

		throw runtime_error(msg);
	}
	return pRes;
}

//////////////////////////////////

blocking_sharedptr MpoFileStream::GetInstance(const char *filename, open_type flags)
{
	blocking_sharedptr pRes;

	mpo_io *io = mpo_open(filename, flags);

	// if file can be opened successfully
	if (io)
	{
		MpoFileStream *pInstance = new MpoFileStream();
		if (pInstance)
		{
			pInstance->m_io = io;
			pRes = blocking_sharedptr(pInstance, MpoFileStream::deleter());
		}
	}
	
	return pRes;
}

blocking_sharedptr MpoFileStream::GetInstance(const wstring &filename, open_type flags)
{
	blocking_sharedptr pRes;

	mpo_io *io = mpo_open(filename.c_str(), flags);

	// if file can be opened successfully
	if (io)
	{
		MpoFileStream *pInstance = new MpoFileStream();
		if (pInstance)
		{
			pInstance->m_io = io;
			pRes = blocking_sharedptr(pInstance, MpoFileStream::deleter());
		}
	}
	
	return pRes;
}

IMpoFileStreamSPtr MpoFileStream::GetInstanceFileStream(const char *filename, open_type flags)
{
	IMpoFileStreamSPtr pRes;

	mpo_io *io = mpo_open(filename, flags);

	// if file can be opened successfully
	if (io)
	{
		MpoFileStream *pInstance = new MpoFileStream();
		if (pInstance)
		{
			pInstance->m_io = io;
			pRes = IMpoFileStreamSPtr(pInstance, MpoFileStream::deleter());
		}
	}

	return pRes;
}

IMpoFileStreamSPtr MpoFileStream::GetInstanceFileStream(const wstring &filename, open_type flags)
{
	IMpoFileStreamSPtr pRes;

	mpo_io *io = mpo_open(filename.c_str(), flags);

	// if file can be opened successfully
	if (io)
	{
		MpoFileStream *pInstance = new MpoFileStream();
		if (pInstance)
		{
			pInstance->m_io = io;
			pRes = IMpoFileStreamSPtr(pInstance, MpoFileStream::deleter());
		}
	}

	return pRes;
}

size_t MpoFileStream::Read(void *buf, size_t stBytesToRead)
{
	MPO_BYTES_READ uBytesRead = 0;
	mpo_read(buf, stBytesToRead, &uBytesRead, m_io);
	return uBytesRead;
}

size_t MpoFileStream::Write(const void *buf, size_t stBytesToWrite)
{
	unsigned int uBytesWritten = 0;
	mpo_write(buf, stBytesToWrite, &uBytesWritten, m_io);
	return uBytesWritten;
}

bool MpoFileStream::Seek(MPO_INT64 i64Offset, seek_type origin)
{
	bool bRes = mpo_seek(i64Offset, origin, m_io);
	return bRes;
}

void MpoFileStream::Truncate(MPO_UINT64 u64FileSize)
{
	bool bRes = mpo_truncate(u64FileSize, m_io);
	if (!bRes)
	{
		throw runtime_error("Truncate failed");
	}
}

MPO_UINT64 MpoFileStream::GetLength()
{
	MPO_UINT64 u64Size = 0;

	bool bSuccess = mpo_get_size(&u64Size, m_io);

	if (!bSuccess)
	{
		// this never happen (the only time I can think of is if m_io is invalid, which it won't be)
		throw runtime_error("mpo_get_size failed");
	}

	return u64Size;
}

MPO_UINT64 MpoFileStream::GetPosition()
{
	MPO_UINT64 u64Pos = 0;

	bool bSuccess = mpo_tell(&u64Pos, m_io);

	if (!bSuccess)
	{
		// this never happen (the only time I can think of is if m_io is invalid, which it won't be)
		throw runtime_error("mpo_tell failed");
	}

	return u64Pos;
}

bool MpoFileStream::CanRead()
{
	return true;
}

bool MpoFileStream::CanWrite()
{
	return true;
}

bool MpoFileStream::CanSeek()
{
	return true;
}

MpoFileStream::MpoFileStream() :
m_io(NULL)
{
}

MpoFileStream::~MpoFileStream()
{
	if (m_io != NULL)
	{
		mpo_close(m_io);
		m_io = NULL;
	}
}
