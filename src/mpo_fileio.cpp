/*
 * mpo_fileio.cpp
 *
 * Copyright (C) 2005 Matthew P. Ownby
 *
 * This file is part of MPOLIB, a multi-purpose library
 *
 * MPOLIB is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPOLIB is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MPO's NOTE:
//  I may wish to use MPOLIB in a proprietary product some day.  Therefore,
//   the only way I can accept other people's changes to my code is if they
//   give me full ownership of those changes.

// mpo_fileio.cpp
// by Matt Ownby

// Replacement functions for the traditional fopen/fread/fclose/etc functions
// Main purpose: to support files larger than 4 gigs
// Secondary purpose: to not rely on MSCVRT dll in windows

#include <mpolib2/mpo_fileio.h>
#include <mpolib2/mpo_misc.h>	// for string conversions
#include "mpo_fileio_internal.h"
#include <stdexcept>

#ifndef WIN32
#include <stdlib.h>	// for malloc

// OSX
#ifdef __APPLE__
#include <sys/param.h>	// for statfs
#include <sys/mount.h>

#else
#include <sys/statvfs.h>	// for statvfs
#endif

#endif

bool mpo_file_exists(const char *filename)
{
	return mpo_file_exists_template(filename);
}

bool mpo_file_exists(const wchar_t *filename)
{
	return mpo_file_exists_template(filename);
}

template<class T> bool mpo_file_exists_template(const T *filename)
{
	bool result = false;
	mpo_io *io = NULL;

	io = mpo_open(filename, MPO_OPEN_READONLY);
	if (io)
	{
		mpo_close(io);
		result = true;
	}
	return result;
}

mpo_io *mpo_open(const char *filename, open_type flags)
{
#ifdef WIN32
	return mpo_open_windows(filename, flags);
#else
	mpo_io *io = NULL;
	bool success = false;

	// dynamically allocate, will be freed by mpo_close
	io = (mpo_io *) malloc(sizeof(mpo_io));

	const char *mode = "rb";	// assume reading
	if (flags == MPO_OPEN_CREATE)
	{
		mode = "wb";
	}
	else if (flags == MPO_OPEN_READWRITE)
	{
		if (mpo_file_exists(filename)) mode = "rb+";	// read/write existing file
		else mode = "wb+";	// create file, open in read/write mode
	}
	else if (flags == MPO_OPEN_APPEND)
	{
		mode = "ab";
	}
	// else unknown... eror
	
	io->handle = MPO_FOPEN(filename, mode);
	if (io->handle)
	{
		MPO_FSEEK(io->handle, 0, SEEK_END);	// go to end of file
		io->size = MPO_FTELL(io->handle);	// get position (total file size)
		MPO_FSEEK(io->handle, 0, SEEK_SET);	// go to beginning
		io->eof = false;
		success = true;
	}

	// if something went wrong
	if (!success)
	{
		free(io);
		io = NULL;
	}

	return io;
#endif

}

mpo_io *mpo_open(const wchar_t *filename, open_type flags)
{
#ifdef WIN32
	return mpo_open_windows(filename, flags);
#else
	try
	{
		// unix uses UTF8 encoding for wide filenames
		string strUTF8 = mpom::ToUTF8Ex(filename);
		return mpo_open(strUTF8.c_str(), flags);
	}
	catch (std::exception &)
	{
		return NULL;
	}
#endif
}

#ifdef WIN32
template<class T> mpo_io *mpo_open_windows(const T *filename, open_type flags)
{
	mpo_io *io = NULL;
	bool success = false;

	// will be freed by mpo_close
	io = (mpo_io *) malloc(sizeof(mpo_io));

	ZeroMemory(io, sizeof(mpo_io));
	io->handle = INVALID_HANDLE_VALUE;
	DWORD dwDesiredAccess = 0;
	DWORD dwShareMode = 0;
	DWORD dwCreationDisposition = 0;
	DWORD dwFlagsAndAttributes = 0;

	if (flags == MPO_OPEN_READONLY)
	{
		dwDesiredAccess = GENERIC_READ;
		dwShareMode = FILE_SHARE_READ;
		dwCreationDisposition = OPEN_EXISTING;
		dwFlagsAndAttributes = FILE_ATTRIBUTE_READONLY;
	}
	else if (flags == MPO_OPEN_READWRITE)
	{
		dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
		dwShareMode = FILE_SHARE_READ;
		dwCreationDisposition = OPEN_ALWAYS;
		dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	}
	else if (flags == MPO_OPEN_CREATE)
	{
		dwDesiredAccess = GENERIC_WRITE;
		dwShareMode = FILE_SHARE_READ;
		dwCreationDisposition = CREATE_ALWAYS;
		dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	}
	else if (flags == MPO_OPEN_APPEND)	// append
	{
		dwDesiredAccess = GENERIC_WRITE;
		dwShareMode = FILE_SHARE_READ;
		dwCreationDisposition = OPEN_ALWAYS;
		dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	}
	else	// unknown value, error
	{
	}

	// wide version
	if (sizeof(T) > 1)
	{
		io->handle = CreateFileW((LPCWSTR) filename, dwDesiredAccess, dwShareMode, NULL,
			dwCreationDisposition, dwFlagsAndAttributes, NULL);
	}
	// ansi version
	else
	{
		io->handle = CreateFileA((LPCSTR) filename, dwDesiredAccess, dwShareMode, NULL,
			dwCreationDisposition, dwFlagsAndAttributes, NULL);
	}

	// if opening the file succeeded
	if (io->handle != INVALID_HANDLE_VALUE)
	{
		// go to the end of the file to be consistent with unix behavior
		if (flags == MPO_OPEN_APPEND)
		{
			mpo_seek(0, MPO_SEEK_END, io);	// seek to the end of the file for appending
		}

		DWORD hSize = 0;
		DWORD lSize = 0;
		lSize = GetFileSize(io->handle, &hSize);
		// if getting the file size succeeded
		if (lSize != INVALID_FILE_SIZE)
		{
			io->size = hSize;	// assign higher 32-bits initially
			io->size <<= 32;	// shift up 32-bits
			io->size |= lSize;	// and merge in lower 32-bit bits
			io->eof = false;
			success = true;
		}
		// else getfilesize failed
	}

	if (!success)
	{
		free(io);
		io = NULL;
	}

	return io;

}
#endif // WIN32

// returns true on success
// EOF is when bytes_read is 0, but true is returned
bool mpo_read (void *buf, size_t bytes_to_read,
			   MPO_BYTES_READ *bytes_read, mpo_io *io)
{
	bool result = false;
	MPO_BYTES_READ backup_bytes_read = 0;	// in case their 'bytes_read' is NULL

	// if they don't care what bytes_read is, then we'll just squelch it
	if (bytes_read == NULL)
	{
		bytes_read = &backup_bytes_read;
	}

#ifdef WIN32
	LPDWORD ptr = bytes_read;
	if (ReadFile(io->handle, buf, (DWORD) bytes_to_read, ptr, NULL) != 0)
	{
		result = true;
	}

#ifdef DEBUG
	else
	{
			char s[160];
			DWORD i = GetLastError();
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, i, 0, s, sizeof(s), NULL);
			int iDontCare = 0;	// put breakpoint here
	}
#endif // DEBUG

#else
	*bytes_read = fread(buf, 1, bytes_to_read, io->handle);
	
	// if we read some bytes, or if we properly got to EOF
	if ((*bytes_read > 0) || ((*bytes_read == 0) || feof(io->handle) != 0))
	{
		result = true;
	}
#endif

	// if we hit EOF, set a flag for convenience
	if ((*bytes_read) != bytes_to_read)
	{
		io->eof = true;
	}

	return result;
}

// returns true on success
bool mpo_write (const void *buf, size_t bytes_to_write, unsigned int *bytes_written, mpo_io *io)
{
	bool result = false;

	unsigned int backup_bytes_written = 0;	// in case their 'bytes_written' is NULL

	// if they don't care what bytes_written is, then we'll just squelch it
	if (bytes_written == NULL)
	{
		bytes_written = &backup_bytes_written;
	}

#ifdef WIN32
	LPDWORD ptr = (LPDWORD) bytes_written;
	if (WriteFile(io->handle, buf, (DWORD) bytes_to_write, ptr, NULL) != 0)
	{
		result = true;
	}
#else
	*bytes_written = fwrite(buf, 1, bytes_to_write, io->handle);
	if (*bytes_written == bytes_to_write)
	{
		result = true;
	}
#endif
	return result;
}

// fseek replacement
// returns true if seek was successful
bool mpo_seek(MPO_INT64 offset, seek_type type, mpo_io *io)
{
	bool result = false;

#ifdef WIN32
	/*
	// This code (SetFilePointerEx) works but only for Win2k and WinXP so I have commented it out
	LARGE_INTEGER l;
	l.QuadPart = offset;
	LARGE_INTEGER pre_result;
	pre_result.QuadPart = -1;

	// if seek is successful
	if (SetFilePointerEx(io->handle, l, &pre_result, type) != 0)
	{
		// result should now contain the current position
	}
	else
	{
		pre_result.QuadPart = -1;
	}

	result = true;	// no decent error checking here
	*/

	LONG loffset = (LONG) offset;	// NOTE : assumes long is 32-bits
	LONG hoffset = (LONG) (offset >> 32);
	DWORD pre_result = 0;

	pre_result = SetFilePointer(io->handle, loffset, &hoffset, type);

	// if we potentially got an error ...
//	if (pre_result == INVALID_SET_FILE_POINTER)
	if (pre_result == -1)	// INVALID_SET_FILE_POINTER is -1 but some old visual studio 6's don't have this defined
	{
		result = false;
		pre_result = GetLastError();	// check to see if we really got an error
		if (pre_result == NO_ERROR)
		{
			result = true;
		}
	}

	// no error
	else
	{
		result = true;
	}

#else
	int pre_result = MPO_FSEEK(io->handle, offset, type);
	if (pre_result == 0) result = true;
#endif

	return result;
}

bool mpo_tell(MPO_UINT64 *pOffset, mpo_io *io)
{
	bool bRes = false;

#ifdef WIN32
	// use an offset of 0 to get the current position
	LONG loffset = 0;
	LONG hoffset = 0;
	DWORD dwRes = 0;

	dwRes = SetFilePointer(io->handle, loffset, &hoffset, FILE_CURRENT);

	// If we got no error
	// (INVALID_SET_FILE_POINTER is -1 but some old visual studio 6's don't have this defined)
	if ((dwRes != -1) || (GetLastError() == NO_ERROR))
	{
		*pOffset = dwRes | (((MPO_UINT64) hoffset) << 32);
		bRes = true;
	}

#else
	*pOffset = MPO_FTELL(io->handle);
	if (*pOffset != (MPO_UINT64) -1)
	{
		bRes = true;
	}
#endif // WIN32

	return bRes;
}

bool mpo_get_size(MPO_UINT64 *pOffset, mpo_io *io)
{
	bool bRes = false;
#ifdef WIN32
	DWORD hSize = 0;
	DWORD lSize = 0;
	lSize = GetFileSize(io->handle, &hSize);
	// if getting the file size succeeded
	if (lSize != INVALID_FILE_SIZE)
	{
		*pOffset = hSize;	// assign higher 32-bits initially
		*pOffset <<= 32;	// shift up 32-bits
		*pOffset |= lSize;	// and merge in lower 32-bit bits
		bRes = true;
	}
	// else getfilesize failed
#else // WIN32
	// seek to end of file, get position, then seek back
	MPO_UINT64 u64CurPos = 0;
	if (!mpo_tell(&u64CurPos, io)) return false;
	if (!mpo_seek(0, MPO_SEEK_END, io)) return false;
	if (!mpo_tell(pOffset, io)) return false;
	if (!mpo_seek(u64CurPos, MPO_SEEK_SET, io)) return false;
	bRes = true;
#endif // non-WIN32

	return bRes;
}

bool mpo_truncate(MPO_UINT64 u64FileSize, mpo_io *io)
{
	bool result = false;
#ifdef WIN32
	// get to the right position
	if (mpo_seek(u64FileSize, MPO_SEEK_SET, io))
	{
		// make this spot the end of the file
		if (SetEndOfFile(io->handle) != 0)
		{
			result = true;
		}
	}
#else
	if (MPO_FTRUNC(fileno(io->handle), u64FileSize) == 0) result = true;
#endif

	// move to beginning of file to have some consistent behavior
	mpo_seek(0, MPO_SEEK_SET, io);

	// update stats
	if (result) io->size = u64FileSize;

	return result;
}

void mpo_close(mpo_io *io)
{
	if (io != NULL)
	{
#ifdef WIN32
		CloseHandle(io->handle);
#else
		fclose(io->handle);
#endif
		io->handle = 0;
		io->eof = false;
		io->size = 0;
		free(io);	// de-allocate
	}
	// else we cannot reference a NULL pointer
}

MPO_UINT64 mpo_get_time_last_modified(mpo_io *io)
{
	MPO_UINT64 u64Result = ~0;

#ifdef WIN32
	FILETIME last_modified;
	if (GetFileTime(io->handle, NULL, NULL, &last_modified))
	{
		u64Result = last_modified.dwHighDateTime; // high part
		u64Result <<= 32;	// shift up 32
		u64Result |= last_modified.dwLowDateTime;	// low part
	}
#else
	// need to call fsync to ensure that the 'last modified time' is up to date
//	if (fsync(fileno(io->handle)) == 0)
//	{
		struct stat teh_stats;	// to get last modified
		if (fstat(fileno(io->handle), &teh_stats) == 0)
		{
			u64Result = teh_stats.st_mtime;
		}
//	}
#ifdef DEBUG
	// else, figure out why fsync failed ...
//	else perror("mpo_get_time_last_modified");
#endif // end DEBUG
#endif
	return u64Result;
}

bool mpo_mkdir(const char *dirname)
{
	bool result = false;

#ifdef WIN32
	if (CreateDirectoryA(dirname, NULL) != 0)
	{
		result = true;
	}
	// else if the directory already exists, it's ok
	else if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		result = true;
	}
	// else some other error, so we fail ...
#else
	if (!mpo_file_exists(dirname))
	{
		// create directory with minimal permissions
		if (mkdir(dirname, 0700) == 0)
		{
			result = true;
		}
#ifdef DEBUG
		else perror("mpo_mkdir");
#endif
	}
	//else the directory already exists, so we don't want to return an error
	else result = true;
#endif

	return result;
}

bool mpo_mkdir(const wchar_t *dirname)
{
	bool result = false;

#ifdef WIN32
	if (CreateDirectoryW(dirname, NULL) != 0)
	{
		result = true;
	}
	// else if the directory already exists, it's ok
	else if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		result = true;
	}
	// else some other error, so we fail ...
#else
	try
	{
		string strUTF8 = mpom::ToUTF8Ex(dirname);
		result = mpo_mkdir(strUTF8.c_str());
	}
	catch (std::exception &)
	{
	}
#endif

	return result;
}

bool mpo_rmdir(const char *dirname)
{
	bool result = false;

#ifdef WIN32
	if (RemoveDirectoryA(dirname) != FALSE)
	{
		result = true;
	}
#ifdef DEBUG
	else
	{
		DWORD dwRes = GetLastError();
		int i = 0;
	}
#endif // DEBUG
#else
	if (rmdir(dirname) == 0)
	{
		result = true;
	}
#endif

	return result;
}

bool mpo_rmdir(const wchar_t *dirname)
{
	bool result = false;

#ifdef WIN32
	if (RemoveDirectoryW(dirname) != FALSE)
	{
		result = true;
	}
#ifdef DEBUG
	else
	{
		DWORD dwRes = GetLastError();
		int i = 0;
	}
#endif // DEBUG
#else
	try
	{
		string strUTF8 = mpom::ToUTF8Ex(dirname);
		result = mpo_rmdir(strUTF8.c_str());
	}
	catch (std::exception &)
	{
	}
#endif

	return result;
}

bool mpo_get_curdir(string &result)
{
	bool success = false;
#ifdef WIN32
	char NPath[MAX_PATH];
	DWORD dwLength = GetCurrentDirectoryA(MAX_PATH, NPath);

	result = string(NPath, dwLength);
	success = true;
#else
	char path[4096];	// PATH_MAX is not reliable in unix, so just choose a high value
	char *pTmp = getcwd(path, sizeof(path));
	if (pTmp != NULL)
	{
		success = true;
		result = string(path);
	}
#endif
	return success;
}

bool mpo_get_curdir(wstring &result)
{
	bool success = false;
#ifdef WIN32
	wchar_t NPath[MAX_PATH];
	DWORD dwLength = GetCurrentDirectoryW(MAX_PATH, NPath);

	result = wstring(NPath, dwLength);
	success = true;
#else
	string strTmp;
	success = mpo_get_curdir(strTmp);
	try
	{
		result = mpom::FromUTF8Ex(strTmp);
	}
	catch (std::exception &)
	{
		success = false;
	}
#endif
	return success;
}

bool mpo_get_freespace(const char *cpszDir, MPO_UINT64 *u64Result)
{
	bool result = false;
#ifdef WIN32
	ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	if (GetDiskFreeSpaceExA(cpszDir, &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes))
	{
		result = true;
		*u64Result = FreeBytesAvailable.QuadPart;
	}
#else

#ifdef __APPLE__
	// this should work on Mac OSX 10.3
	struct statfs s;
	if (statfs(cpszDir, &s) == 0)
	{
		result = true;
		*u64Result = ((MPO_UINT64) s.f_bsize) * ((MPO_UINT64) s.f_bavail);
	}
#else
	// linux method, may not work with all UNIX's
	struct statvfs s;
	if (statvfs(cpszDir, &s) == 0)
	{
		result = true;
		*u64Result = ((MPO_UINT64) s.f_bsize) * ((MPO_UINT64) s.f_bavail);
	}
#endif

#endif // NOT WIN32
	return result;
}

bool mpo_get_freespace(const wchar_t *cpszDir, MPO_UINT64 *u64Result)
{
	bool result = false;
#ifdef WIN32
	ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	if (GetDiskFreeSpaceExW(cpszDir, &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes))
	{
		result = true;
		*u64Result = FreeBytesAvailable.QuadPart;
	}
#else
	try
	{
		string strUTF8 = mpom::ToUTF8Ex(cpszDir);
		result = mpo_get_freespace(strUTF8.c_str(), u64Result);
	}
	catch (std::exception &)
	{
	}
#endif // NOT WIN32
	return result;
}

bool mpo_delete(const char *cpszFileName)
{
	bool bRes = false;

#ifdef WIN32
	bRes = (DeleteFileA(cpszFileName) != 0);
#ifdef DEBUG
	DWORD dwRes = GetLastError();
	int i = 0;
#endif // DEBUG
#else
	if (unlink(cpszFileName) == 0)
	{
		bRes = true;
	}
#endif // WIN32

	return bRes;
}

bool mpo_delete(const wchar_t *cwpszFileName)
{
	bool bRes = false;

#ifdef WIN32
	bRes = (DeleteFileW(cwpszFileName) != 0);
#else
	try
	{
		string strFileName = mpom::ToUTF8Ex(cwpszFileName);
		bRes = mpo_delete(strFileName.c_str());
	}
	catch (std::exception &)
	{
	}
#endif

	return bRes;
}

bool mpo_move(const char *cpszDstName, const char *cpszSrcName)
{
	bool bRes = false;

#ifdef WIN32
	bRes = (MoveFileA(cpszSrcName, cpszDstName) != 0);
#else
	bRes = (rename(cpszSrcName, cpszDstName) == 0);
#endif

	return bRes;
}

bool mpo_move(const wchar_t *cpszDstName, const wchar_t *cpszSrcName)
{
	bool bRes = false;

#ifdef WIN32
	bRes = (MoveFileW(cpszSrcName, cpszDstName) != 0);
#else
	try
	{
		string strDst = mpom::ToUTF8Ex(cpszDstName);
		string strSrc = mpom::ToUTF8Ex(cpszSrcName);
		bRes = mpo_move(strDst.c_str(), strSrc.c_str());
	}
	catch (std::exception &)
	{
	}
#endif

	return bRes;
}

////////////////////////////////////////////

//////////////////////////////////////////////////////

IMpoFileIOSPtr MpoFileIOFactory::CreateInstance()
{
    return MpoFileIO::CreateInstance();
}

IMpoFileIOSPtr MpoFileIO::CreateInstance()
{
    return IMpoFileIOSPtr(new MpoFileIO(), MpoFileIO::deleter());
}

bool MpoFileIO::FileExists(const wstring &wstrFileExists)
{
    return mpo_file_exists(wstrFileExists.c_str());
}

void MpoFileIO::MkDir(const wstring &wstrDirName)
{
    if (!mpo_mkdir(wstrDirName.c_str()))
    {
        throw runtime_error("MkDir failed");
    }
}

void MpoFileIO::RmDir(const wstring &wstrDirName)
{
    if (!mpo_rmdir(wstrDirName.c_str()))
    {
        throw runtime_error("RmDir failed");
    }
}

MPO_UINT64 MpoFileIO::GetFreeBytes(const wstring &wstrDirName)
{
    MPO_UINT64 u64Res;
    if (!mpo_get_freespace(wstrDirName.c_str(), &u64Res))
    {
        throw runtime_error("Get free space failed");
    }
    return u64Res;
}

void MpoFileIO::Delete(const wstring &wstrFileName)
{
    if (!mpo_delete(wstrFileName.c_str()))
    {
        throw runtime_error("Delete failed");
    }
}

MpoFileIO::MpoFileIO()
{
}

MpoFileIO::~MpoFileIO()
{
}
