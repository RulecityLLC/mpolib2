#ifndef MPO_TYPES_H
#define MPO_TYPES_H

#include <stdio.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>

typedef unsigned __int64 MPO_UINT64;
typedef __int64 MPO_INT64;
#else
typedef unsigned long long MPO_UINT64;
typedef long long MPO_INT64;
#endif

// ways that file can be opened
typedef enum
{
	MPO_OPEN_READONLY,	// opens pre-existing file in read-only mode
	MPO_OPEN_READWRITE,	// opens pre-existing file in read/write mode, or creates file if it does not exist
	MPO_OPEN_CREATE,	// creates/overwrites file, for writing purposes
	MPO_OPEN_APPEND,	// creates/appends file, for writing purposes
} open_type;

typedef enum
{
#ifdef WIN32
		MPO_SEEK_SET = FILE_BEGIN,
		MPO_SEEK_CUR = FILE_CURRENT,
		MPO_SEEK_END = FILE_END
#else
		MPO_SEEK_SET = SEEK_SET,
		MPO_SEEK_CUR = SEEK_CUR,
		MPO_SEEK_END = SEEK_END
#endif
} seek_type;


#endif // MPO_TYPES_H
