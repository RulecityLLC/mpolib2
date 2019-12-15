//
// Created by Matt on 10/4/2019.
//

#ifndef MPO2_MPO_FILEIO_INTERNAL_H
#define MPO2_MPO_FILEIO_INTERNAL_H

#include <mpolib/mpo_fileio.h>
#include <mpolib/mpo_deleter.h>

#include <string>
using namespace std;

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <stdio.h>
#include <sys/stat.h>	// for fstat
#include <unistd.h>	// for ftruncate
#include <sys/types.h>	// for ftruncate
#endif

#include <mpolib/mpo_types.h>	// for MPO_UINT64 definition

#ifdef WIN32
#define MPO_HANDLE HANDLE
#define MPO_BYTES_READ DWORD
#else
// unix
#define MPO_HANDLE FILE *
#define MPO_BYTES_READ size_t

// Mac OSX
#ifdef __APPLE__
#define MPO_FOPEN fopen
#define MPO_FSEEK fseeko
#define MPO_FTELL ftello
#define MPO_FTRUNC ftruncate
#else
// linux
#define MPO_FOPEN fopen64
#define MPO_FSEEK fseeko64
#define MPO_FTELL ftello64
#define MPO_FTRUNC ftruncate64
#endif // unix

#endif

// macro that sets io pointer to NULL after closing the file, which is useful for
//  determining whether an io pointer either is open or closed (without worrying if it is undefined)
#define MPO_SAFE_CLOSE(ptr) mpo_close(ptr); ptr = NULL;

struct mpo_io
{
    MPO_HANDLE handle;	// handle of the file (keep this at the beginning of the struct to make it easy to statically initialize)
    MPO_UINT64 size;	// the size of the file (WARNING : may only indicate size of file when it was opened)
    bool eof;	// whether we have reached the End-Of-File
};

EXPORT_ME void mpo_test();
EXPORT_ME bool mpo_file_exists(const char *filename);
EXPORT_ME bool mpo_file_exists(const wchar_t *filename);
template<class T> bool mpo_file_exists_template(const T *filename);

// returns a pointer to an mpo_io structure if successful, NULL if unsuccessful
EXPORT_ME mpo_io *mpo_open(const char *filename, open_type flags);
EXPORT_ME mpo_io *mpo_open(const wchar_t *filename, open_type flags);
#ifdef WIN32
template<class T> mpo_io *mpo_open_windows(const T *pFilename, open_type flags);
#endif // WIN32

EXPORT_ME bool mpo_read (void *buf, size_t bytes_to_read, MPO_BYTES_READ *bytes_read, mpo_io *io);
EXPORT_ME bool mpo_write (const void *buf, size_t bytes_to_write, unsigned int *bytes_written, mpo_io *io);
EXPORT_ME bool mpo_seek(MPO_INT64 offset, seek_type type, mpo_io *io);

// Gets current file position.
// Stores result in *pOffset and returns true on success.
EXPORT_ME bool mpo_tell(MPO_UINT64 *pOffset, mpo_io *io);

// Gets the current file size
// Stores result in *pOffset and returns true on success.
EXPORT_ME bool mpo_get_size(MPO_UINT64 *pOffset, mpo_io *io);

// sets an open file's size to the size indicated by 'u64FileSize' and returns true on success
EXPORT_ME bool mpo_truncate(MPO_UINT64 u64FileSize, mpo_io *io);

EXPORT_ME void mpo_close(mpo_io *io);

// Gets a unit of time representing when this file was last modified.
// It will mean something different in win32 and linux and thus is only useful for
// comparing against other files to see which one was modified most recently.
// (returns -1 on error)
EXPORT_ME MPO_UINT64 mpo_get_time_last_modified(mpo_io *io);

// Attempts to create a directory (with permissions such that only the user can access that dir)
// 'dirname' is the name of the directory to create, returns on true on success
EXPORT_ME bool mpo_mkdir(const char *dirname);
EXPORT_ME bool mpo_mkdir(const wchar_t *dirname);

// Attempts to remove a directory (directory must be empty).
EXPORT_ME bool mpo_rmdir(const char *dirname);
EXPORT_ME bool mpo_rmdir(const wchar_t *dirname);

EXPORT_ME bool mpo_get_curdir(string &result);
EXPORT_ME bool mpo_get_curdir(wstring &result);

// Attempts to determine how much free disk space is available (in bytes!) to the current user if they
//  were to write to the directory indicated by 'cpszDir'.  On success, the amount of free
//  space is stored in 'u64Result' and true is returned, otherwise false is returned on error.
EXPORT_ME bool mpo_get_freespace(const char *cpszDir, MPO_UINT64 *u64FreeBytes);
EXPORT_ME bool mpo_get_freespace(const wchar_t *cpszDir, MPO_UINT64 *u64FreeBytes);

// deletes a file, returns true on success
EXPORT_ME bool mpo_delete(const char *cpszFileName);
EXPORT_ME bool mpo_delete(const wchar_t *cwpszFileName);

// moves/renames a file
EXPORT_ME bool mpo_move(const char *cpszDstName, const char *cpszSrcName);
EXPORT_ME bool mpo_move(const wchar_t *cpszDstName, const wchar_t *cpszSrcName);

class MpoFileIOInternal : public IMpoFileIO, public MpoDeleter
{
public:
static IMpoFileIOSPtr CreateInstance();

bool FileExists(const wstring &wstrFileExists);

void MkDir(const wstring &wstrDirName);

void RmDir(const wstring &wstrDirName);

MPO_UINT64 GetFreeBytes(const wstring &wstrDirName);

void Delete(const wstring &wstrFileName);

private:
    MpoFileIOInternal();
	virtual ~MpoFileIOInternal();

	void DeleteInstance() { delete this; }
};

#endif //MPO2_MPO_FILEIO_INTERNAL_H
