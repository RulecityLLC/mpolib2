/*
 * mpo_misc.h
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

// misc.h
// by Matt Ownby

#ifndef MISC_H
#define MISC_H

#include "mpo_dll.h"
#include "mpo_types.h"	// for MPO_UINT64
#include <wchar.h>
#include <string>
#include <list>

using namespace std;

// if we're on C++11, then unique_ptr is standard
#if __cplusplus >= 199711L

#include <memory>
#define SHARED_ARRAY(T) std::unique_ptr<T []>

#else

#include <boost/shared_array.hpp>
using namespace boost;

#define SHARED_ARRAY(T) boost::shared_array<T>

#endif


#if WCHAR_MAX > 0xFFFFu
typedef wchar_t mpo_wchar_t;
#define TO_MPO_WSTR(a) a
#else
typedef unsigned int mpo_wchar_t;
#define TO_MPO_WSTR(a) mpom::str_conv_mpo(a)
#endif

// real wide string (to compensate for microsoft's 2-byte wchar_t)
typedef basic_string<mpo_wchar_t, char_traits<mpo_wchar_t>,
	allocator<mpo_wchar_t> > mpo_wstring;

// Class to be used to allocate memory (does not check for buffer overflows, but will automatically de-allocate the RAM)
// Main purpose is to prevent memory leaks.
class EXPORT_ME mpo_buf
{
public:
	mpo_buf();
	~mpo_buf();

	// copy constructor
	mpo_buf(const mpo_buf &src);

	// assignment from another buf
	mpo_buf& operator=(const mpo_buf &src);

	// assignment from an STL string
	mpo_buf& operator=(const string &strSrc);

	// assignment from C-string
	mpo_buf& operator=(const char *cpszSrc);

	bool operator==(const mpo_buf &src) const;

	bool operator!=(const mpo_buf &src) const;

	bool alloc(unsigned int uSize);

	// returns writable pointer (warning, this does not protect against buffer overflow!)
	unsigned char *data();

	// returns read-only pointer
	const unsigned char *data_const() const;

	unsigned int size() const;
private:
	void copy(const void *src, unsigned int uSize);
	void dealloc();

	unsigned char *m_ptr;
	unsigned int m_uSize;
};

// mpom stands for MPO Misc class
class EXPORT_ME mpom
{

public:

	// returns an upper-case version of a string
	static string str_toupper(const string &src);

	// returns an lower-case version of a string
	static string str_tolower(const string &src);

	// returns true if two strings are equal (case-insensitive)
	static bool str_case_eq(const string &s1, const string &s2);
	
	// function that converts an ASCII STL string into a wide string
	static wstring str_conv(const string &strSrc);

	// function that converts an ASCII C-string into a wide string
	static wstring str_conv(const char *cpszSrc);

	// function that converts a wide string to an ASCII STL string
	static string str_conv(const wstring &strSrc);

	// function that converts a wide constant C-string to an ASCII STL string
	static string str_conv(const wchar_t *strSrc);

	// This function should not be called directly, but the macro TO_MPO_WSTR should be used instead.
	// This is because on GNU, mpo_wstring is the same as wstring.
#if WCHAR_MAX <= 0xFFFFu
	// converts wstring to mpo_wstring (which will always succeed, so no error checking is needed)
	static mpo_wstring str_conv_mpo(const wstring &wpszSrc);
#endif

	// converts mpo_string to wstring if no data loss will occur, otherwise returns false
	static bool ToWStr(wstring &wstrDst, const mpo_wstring &wstrSrc);

	// Converts to/from UTF8, returning true if successful.
	// (it's possible for this conversion to fail if the request violates the UTF-8 standard, RFC3629)
	static bool ToUTF8(string &strDst, const mpo_wstring &wstrSrc);

	static string ToUTF8Ex(const mpo_wstring &wstrSrc);	// throws an exception on failure, otherwise returning the result

	// this function should only be defined if mpo_wstring is different from wstring
#if WCHAR_MAX <= 0xFFFFu
	static string ToUTF8Ex(const wstring &wstrSrc);	// throws an exception on failure, otherwise returning the result
#endif

	static bool FromUTF8(mpo_wstring &wstrDst, const string &strSrc);

	static mpo_wstring FromUTF8Ex(const string &strSrc);	// throws an exception on failure, otherwise returning the result

	// convenience function for converting directly from UTF8 to wstring
	static wstring FromUTF8ExW(const string &strSrc);	// throws an exception on failure, otherwise returning the result

	// function that puts a path+filename in a standard form to make string comparisons possible
	static string standardize_path(const string &Path);

	// wide version
	static wstring standardize_path(const wstring &Path);

	// converts binary data to LOWER-CASE ascii hex
	static string bin2hex(const void *data_v, unsigned int len);

	// converts ASCII HEX (s1) into binary data (strBin)
	// CAVEAT : strASCIIHex's length must be a multiple of 8
	// Returns true on success
	static bool hex2bin(const string &strASCIIHex, string &strBin);

	// just a different version of the previous hex2bin
	static bool hex2bin(const string &strASCIIHex, mpo_buf &buf);

	// Returns true if the character is whitespace
	// If 'bOnlySpaceAndTab' is true, it only checks for spaces (' ') and tabs ('\t')
	static bool isspace(char ch, bool bOnlySpaceAndTab);

	// returns the first white-space separated word in 'strBuf' and removes the returned word
	//  from the buffer (the whitespace is also removed).
	static string get_first_word(string &strBuf);

	// returns the first line (ending in CR or LF) in 'strBuf' WITHOUT the trailing CR/LF,
	//  and removes the returned line from 'strBuf'
	static string get_first_line(string &strBuf);

	// trims whitespace off the front end back of a string
	static void trim(string &s);

	// for those that want to use trim without altering the original string :)
	static string trim(const string &s);

	// computes an MD5 hash on 'data' of length 'len' returning a lower-case ASCII string as a result
	static string compute_md5(const unsigned char *data, unsigned int len);

	// returns the filename from the path
	// (ie returns "blah.txt" from "c:\temp\blah.txt" or "/mnt/stuff/blah.txt")
	static string get_file_from_path(const string &strPath);
	static wstring get_file_from_path(const wstring &strPath);

	// returns the directory portion of a full path (without trailing slash)
	// (ie returns "c:/temp" from "c:/temp/blah.txt")
	// If there is no directory, it returns an empty string
	static wstring get_dir_from_path(const wstring &wstrPath);

	// Creates a folder in a writable place relative to the platform (for example, on Windows it would be beneath the user's APPDATA area).
	// Returns true if folder was created (or already existed) and stores the full path to the folder in w/strFullPath.
	static bool create_user_data_folder(string &strFullPath, const string &strFolderName);
	static bool create_user_data_folder(wstring &wstrFullPath, const wstring &wstrFolderName);

	// helper function to write to memory in little endian format
	static void write_lile32(void *pBuf, unsigned int u);

	// helper function to write to memory in little endian format
	static void write_lile64(void *pBuf, MPO_UINT64 u);

	// helper function to read from memory in little endian format
	static unsigned int read_lile32(void *pBuf);

	// helper function to read from memory in little endian format
	static MPO_UINT64 read_lile64(void *pBuf);

	// helper function to convert unsigned ints to a big array char array
	static string uint2bige32(unsigned int u);

	static string uint2bige64(MPO_UINT64 u);

	// helper function to convert unsigned ints to a big array char array
	// (put into the .h file for VS6's benefit)
	template <class T> static string uint2bige(T u)
	{
		string result = "";

		for (unsigned int i = 0; i < sizeof(T); i++)
		{
			result += (char) (u >> ((sizeof(T)-1) << 3));
			u <<= 8;
		}
		return result;
	}

	// helper function to convert big endian char array to uint32
	static unsigned int bige2uint32(const void *buf);

	// helper function to convert big endian char array to uint64
	static MPO_UINT64 bige2uint64(const void *buf);

	// helper function to convert big endian char array to uint
	// (put into the .h file for VS6's benefit)
	template <class T> static void bige2uint(const void *buf, T &result)
	{
		result = 0;

		for (unsigned int i = 0; i < sizeof(T); i++)
		{
			result <<= 8;
			result |= ((unsigned char *) buf)[i];
		}
	}
};

// WAITR_* prefix used so as to not conflict with definitions in windows.h
enum waitr
{
	WAITR_ERROR, WAITR_FINISHED, WAITR_BUSY
};

#ifdef WIN32
typedef HANDLE MPO_PID;
#else
typedef pid_t MPO_PID;
#endif

// spawns a separate process, while leaving main process running
EXPORT_ME bool spawn_executable(list <string> cmd_line, MPO_PID &pid);

// waits for spawned process to complete
EXPORT_ME waitr wait_executable(MPO_PID pid, int *exit_code, unsigned int uTimeoutMs);

// prepares arg to be used in a win32 command-line, which means if it has a space in it, it gets surrounded by ""'s and any "'s get removed
EXPORT_ME string escape_arg(const string &arg);

#endif // MISC_H

