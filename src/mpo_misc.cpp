/*
* mpo_misc.cpp
*
* Copyright (C) 2019 Matthew P. Ownby
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

#include <string.h>
#include <mpolib2/mpo_misc.h>
#include "md5_global.h"
#include "md5.h"
#include <mpolib2/mpo_numstr.h>
#include <mpolib2/mpo_fileio.h>
#include <stdexcept>
#include "mpo_fileio_internal.h"

#ifdef WIN32
// get rid of security warnings for wide string conversion functions
#pragma warning(disable: 4996)
#include <shlobj.h>	// for shell folder stuff
#else
// for waitpid
#include <sys/types.h>
#include <sys/wait.h>
#endif

#ifdef DEBUG
#include <assert.h>
#include <iostream>
// namespace has already been declared in header
#endif

string mpom::str_tolower(const string &src)
{
	string result = "";
	char ch = 0;
	for (size_t i = 0; i < src.size(); i++)
	{
		ch = src[i];
		if ((ch >= 'A') && (ch <= 'Z'))
		{
			ch |= 0x20;	// setting bit 5 converts uppercase to lowercase
		}
		result += ch;
	}
	return result;
}

string mpom::str_toupper(const string &src)
{
	string result = "";
	char ch = 0;
	for (size_t i = 0; i < src.size(); i++)
	{
		ch = src[i];
		if ((ch >= 'a') && (ch <= 'z'))
		{
			ch &= 0xDF;	// clearing bit 5 converts lowercase to uppercase
		}
		result += ch;
	}
	return result;
}

bool mpom::str_case_eq(const string &s1, const string &s2)
{
	return (str_toupper(s1) == str_toupper(s2));
}

// function that converts an ASCII STL string into a wide STL
wstring mpom::str_conv(const string &strSrc)
{
	return str_conv(strSrc.c_str());
}

wstring mpom::str_conv(const char *cpszSrc)
{
	size_t stChars = mbstowcs(NULL, cpszSrc, 0);
	stChars++;	// make room for null terminator
    SHARED_ARRAY(wchar_t) BufSA(new wchar_t[stChars]);
	wchar_t *pBuf = BufSA.get();

	stChars = mbstowcs(pBuf, cpszSrc, stChars);

	return wstring(pBuf, stChars);
}

// function that converts a wide STL string to an ASCII STL string
string mpom::str_conv(const wstring &strSrc)
{
	return str_conv(strSrc.c_str());
}

// function that converts a wide C-string to an ASCII STL string
string mpom::str_conv(const wchar_t *strSrc)
{
	string strRes;
    SHARED_ARRAY(char) BufSA;
	char *pBuf;
	size_t stChars = wcstombs(NULL, strSrc, 0);

	// if conversion can succeed
	if (stChars != (size_t) -1)
	{
		stChars++;	// make room for null terminator
		BufSA.reset(new char[stChars]);
		pBuf = BufSA.get();

		stChars = wcstombs(pBuf, strSrc, stChars);
	}

	// if conversion succeeded
	if (stChars != (size_t) -1)
	{
		strRes = string(pBuf, stChars);
	}
	// else conversion failed, so fallback to a cheesy method
	else
	{
		const wchar_t *ptr = strSrc;
		int i = 0;
		while (ptr[i] != 0)
		{
			wchar_t ch = ptr[i];

			// if character is 8-bit
			if (ch <= 255)
			{
				strRes += (char) ch;
			}
			// else discard it because it may be what caused the conversion to fail

			i++;
		}
	}

	return strRes;
}

#if WCHAR_MAX <= 0xFFFFu
mpo_wstring mpom::str_conv_mpo(const wstring &wpszSrc)
{
	mpo_wstring wstrRes;
	int i = 0;
	while (wpszSrc[i] != 0)
	{
		wstrRes.push_back(wpszSrc[i]);
		i++;
	}
	return wstrRes;
}
#endif // _MSC_VER

bool mpom::ToWStr(wstring &wstrDst, const mpo_wstring &wstrSrc)
{
	bool bRes = true;
	wstring wstrRes;

	for (mpo_wstring::const_iterator si = wstrSrc.begin();
		si != wstrSrc.end(); si++)
	{
		mpo_wchar_t ch = *si;

		// if the character will fit
		if (ch <= 0xFFFF)
		{
			wstrDst.push_back((wchar_t) ch);
		}
		else
		{
			bRes = false;
			break;
		}
	}	
	return bRes;
}

bool mpom::ToUTF8(string &strDst, const mpo_wstring &wstrSrc)
{
	bool bRes = true;

	strDst.clear();
	for (mpo_wstring::const_iterator wi = wstrSrc.begin(); wi != wstrSrc.end(); wi++)
	{
		// this is an unsigned int instead of a wchar_t to avoid a warning on MSVC compilers since wchar_t is only 2 bytes there
		unsigned int ch = *wi;

		// single octet (ASCII)
		if (ch <= 0x7F)
		{
			strDst += (char) ch;
		}
		// two octets
		else if (ch <= 0x7FF)
		{
			strDst += (char) (0xC0 | (ch >> 6));
			strDst += (char) (0x80 | (ch & 0x3F));
		}
		// illegal range
		else if ((ch >= 0xD800) && (ch <= 0xDFFF))
		{
			bRes = false;
		}
		// three octets
		else if (ch <= 0xFFFF)
		{
			strDst += (char) (0xE0 | (ch >> 12));
			strDst += (char) (0x80 | ((ch >> 6) & 0x3F));
			strDst += (char) (0x80 | (ch & 0x3F));
		}
		// four octets
		else if (ch <= 0x10FFFF)
		{
			strDst += (char) (0xF0 | (ch >> 18));
			strDst += (char) (0x80 | ((ch >> 12) & 0x3F));
			strDst += (char) (0x80 | ((ch >> 6) & 0x3F));
			strDst += (char) (0x80 | (ch & 0x3F));
		}
		// else out of range
		else
		{
			bRes = false;
		}
	}

	return bRes;
}

string mpom::ToUTF8Ex(const mpo_wstring &wstrSrc)
{
	string strRes;
	if (!ToUTF8(strRes, wstrSrc))
	{
		throw std::runtime_error("Unable to convert to UTF8");
	}
	return strRes;
}

#if WCHAR_MAX <= 0xFFFFu
string mpom::ToUTF8Ex(const wstring &wstrSrc)
{
	return ToUTF8Ex(str_conv_mpo(wstrSrc.c_str()));
}
#endif // _MSC_VER

bool mpom::FromUTF8(mpo_wstring &wstrDst, const string &strSrc)
{
	bool bRes = true;

	wstrDst.clear();

	// special case: if the src is empty, then the destination will be empty too
	if (strSrc.empty())
	{
		return true;
	}

	// determine how many octets the sequence has
	string::const_iterator si = strSrc.begin();
	while (si != strSrc.end())
	{
		unsigned char ch = *si;
		unsigned int uRange = 0;
		unsigned int uIterations = 0;
		mpo_wchar_t u = 0;

		// 1 octet (ASCII)
		if ((ch & 0x80) == 0)
		{
			u = ch;
		}
		// 2 octets
		else if ((ch & 0xE0) == 0xC0)
		{
			u = ch & 0x1F;
			uIterations = 1;
			uRange = 0x80;
		}
		// 3 octets
		else if ((ch & 0xF0) == 0xE0)
		{
			u = ch & 0x0F;
			uIterations = 2;
			uRange = 0x800;
		}
		// 4 octets
		else if ((ch & 0xF8) == 0xF0)
		{
			u = ch & 0x07;
			uIterations = 3;
			uRange = 0x10000;
		}
		// not UTF8, bail out!
		else
		{
			bRes = false;
			break;
		}

		for (unsigned int i = 0; i < uIterations; i++)
		{
			u <<= 6;
			si++;
			if (si == strSrc.end())
			{
				bRes = false;
				break;
			}
			u |= (*si & 0x3F);
		}

		// range check, make sure this isn't overly long (RFC requires us to check this)
		if ((unsigned int) u < uRange)
		{
			bRes = false;
			break;
		}

		wstrDst.push_back(u);

		si++;
	}

	return bRes;
}

mpo_wstring mpom::FromUTF8Ex(const string &strSrc)
{
	mpo_wstring wstrRes;
	if (!FromUTF8(wstrRes, strSrc))
	{
		throw std::runtime_error("Unable to convert from UTF8");
	}
	return wstrRes;
}

wstring mpom::FromUTF8ExW(const string &strSrc)
{
	mpo_wstring wstrResMpo = FromUTF8Ex(strSrc);
	wstring wstrRes;
	if (!ToWStr(wstrRes, wstrResMpo))
	{
		throw std::runtime_error("wstring does not have enough precision for correct UTF8 conversion");
	}
	return wstrRes;
}

template <class _ch> basic_string<_ch> standardize_path_template(const basic_string<_ch> &Path)
{
	bool bGotSep = false;
	basic_string<_ch> spath;
	
	size_t stIdx = 0;
	while (stIdx < Path.size())
	{
		// if we have a redundant current directory, then get rid of it to make string comparisons work properly
		if (Path[stIdx] == '.')
		{
			bool bCurDirFound = false;

			stIdx++;	// move ahead one to see if this is really a curdir or not...
			
			// toss out trailing separator that comes after curdir
			while ((stIdx < Path.size()) && ((Path[stIdx] == '/') || (Path[stIdx] == '\\')))
			{
				bCurDirFound = true;
				stIdx++;
			}

			// if it's a false alarm and this isn't a curdir
			if (!bCurDirFound)
			{
				// false alarm
				spath += '.';
			}
		}
		// if it's not part of a redundant directory, then use it ...
		else
		{
			_ch ch = Path[stIdx];

			// if it's not a separator, then add it
			if ((ch != '/') && (ch != '\\'))
			{
				bGotSep = false;
				spath += ch;
			}
			// else if it's a separator, and we didn't get a separator last time
			else if (!bGotSep)
			{
				bGotSep = true;
				spath += '/';	// this char works on win32 also, so we use it everywhere
			}
			// else we got a separator last time, so drop this one
			stIdx++;
		}
	}

	// this is a cheesy way to create the string "/." so it works for both strings and wstrings
	basic_string<_ch> strTrailer;
	strTrailer += '/';
	strTrailer += '.';

	// if path ends in redundant curdir, then get rid of it
	if ((spath.size() > 1) && (spath.substr(spath.size() - 2) == strTrailer))
	{
		spath.erase(spath.size() - 2);
	}

	return spath;
}

// returns a standard form of the inputted path to make string comparisons possible
string mpom::standardize_path(const string &Path)
{
	return standardize_path_template<char>(Path);
}

wstring mpom::standardize_path(const wstring &Path)
{
	return standardize_path_template<wchar_t>(Path);
}

string mpom::bin2hex(const void *data_v, unsigned int len)
{
	const char *HEXASC = { "0123456789abcdef" };
	string result = "";
	const unsigned char *data = (const unsigned char *) data_v;

	for (unsigned int i = 0; i < len; i++)
	{
		result += HEXASC[(data[i] >> 4)];
		result += HEXASC[(data[i] & 0xF)];
	}
	return result;
}

bool mpom::hex2bin(const string &strASCIIHex, string &strBin)
{
	bool result = false;

	strBin = "";

	// make sure it is a multiple of 8
	if ((strASCIIHex.size() % 8) == 0)
	{
		// split value into 32-bit chunks
		for (size_t i = 0; i < strASCIIHex.size(); i += 8)
		{
			string s4 = strASCIIHex.substr(i, 8);
			unsigned int u = numstr::ToUint32(s4.c_str(), 16);

			// add resulting 32-bit value to info hash
			// (slower, but done to ensure byte order is correct)
			for (size_t j = 0; j < 4; j++)
			{
				strBin += ((char) (u >> 24));
				u <<= 8;
			}
		}
		result = true;	// we presumably have succeeded :)
	}
	// else it's messed up, so just toss it out

	return result;
}

// conversion function
bool mpom::hex2bin(const string &strASCIIHex, mpo_buf &buf)
{
	string sbinBuf = "";
	bool bResult = hex2bin(strASCIIHex, sbinBuf);
	buf = sbinBuf;
	return bResult;
}

bool mpom::isspace(char ch, bool bOnlySpaceAndTab)
{
	bool bResult = false;

	if ((ch == ' ') || (ch == '\t')) bResult = true;

	// if we can check for other whitespace characters ...
	if (!bOnlySpaceAndTab)
	{
		// check for form-feed, newline, carriage return, and vertical tab
		if ((ch == '\f') || (ch == '\n') || (ch == '\r') || (ch == '\v'))
		{
			bResult = true;
		}
	}

	return bResult;
}

string mpom::get_first_word(string &strBuf)
{
	string strRes = "";
	size_t stIdx = 0;

	// skip preceeding whitespace
	for (;;)
	{
		// make sure we don't overflow
		if (stIdx >= strBuf.size())
		{
			break;
		}

		// if current character is not whitespace, then break...
		if (!isspace(strBuf[stIdx], false))
		{
			break;
		}

		++stIdx;
	} // loop while we have preceeding whitespace

	// copy all non-whitespace into result
	for (;;)
	{
		// protect against overflow
		if (stIdx >= strBuf.size())
		{
			break;
		}

		// if current character is whitespace, then break
		if (isspace(strBuf[stIdx], false))
		{
			break;
		}

		strRes += (char) strBuf[stIdx];	// add to result ...
		++stIdx;
	}

	// if we got something, then we can remove it from the strBuf string ...
	if (!strRes.empty())
	{
		strBuf.erase(0, stIdx);
	}
	// else we couldn't extract a word, so don't change anything ...

	return strRes;
}

string mpom::get_first_line(string &strBuf)
{
	string strRes = "";

	size_t stLF = strBuf.find(10);
	size_t stCR = strBuf.find(13);

	// find the first CR/LF character
	size_t stIdx = min(stLF, stCR);

	// if there is a CR or LF in strBuf
	if (stIdx != string::npos)
	{
		strRes = strBuf.substr(0, stIdx);
		strBuf.erase(0, stIdx);	// erase the line we just extracted

		// get rid of any trailing CR/LF characters ...
		for (;;)
		{
			// prevent overflow
			if (stIdx >= strBuf.size())
			{
				break;
			}

			// if it's not a CR/LF then leave it alone ...
			if ((strBuf[0] != 10) && (strBuf[0] != 13))
			{
				break;
			}

			strBuf.erase(0, 1);	// get rid of CR/LF
		}
	}
	// else there's no CR/LF so strBuf IS the first line
	else
	{
		strRes = strBuf;
		strBuf.clear();
	}

	return strRes;
}

void mpom::trim(string &s)
{
	size_t index = 0;
	// get rid of leading whitespace
	while ((isspace(s[index], true)) && (index < s.size()))
	{
		index++;
	}

	// if we found any leading whitespace ...
	if (index > 0)
	{
		s.erase(0, index);	// get rid of it ...

		// now check for trailing whitespace
		index = s.size()-1;

		// move backward until we find non-whitespace, which we _will_ do eventually
		while (isspace(s[index], true))
		{
			index--;
		}
		// if there was some trailing whitespace
		if (index < (s.size()-1))
		{
			s.erase(index+1);	// get rid of everything from it until the end
		}
	}
}

string mpom::trim(const string &s)
{
	string strResult = s;
	trim(strResult);
	return strResult;
}

string mpom::compute_md5(const unsigned char *data, unsigned int len)
{
	oMD5_CTX ctx;
	unsigned char digest[16];
	MD5Init(&ctx);
	MD5Update(&ctx, data, len);
	MD5Final(digest, &ctx);
	return(bin2hex(digest, sizeof(digest)));
}

string mpom::get_file_from_path(const string &strPath)
{
	string strResult = strPath;

	for (size_t idx = 0; idx < strPath.size(); idx++)
	{
		// go backward
		size_t ridx = strPath.size() - idx - 1;	// our real index

		// if we find a matching character
		if ((strPath[ridx] == '/') || (strPath[ridx] == '\\'))
		{
			// toss away path
			strResult.erase(0, ridx + 1);
			break;
		}
	}

	return strResult;
}

wstring mpom::get_file_from_path(const wstring &wstrPath)
{
	wstring wstrResult = standardize_path(wstrPath);
	size_t stIdx = wstrResult.find_last_of('/');

	// if we find a leading folder
	if (stIdx != wstring::npos)
	{
		wstrResult = wstrResult.substr(stIdx+1);
	}

	return wstrResult;
}

wstring mpom::get_dir_from_path(const wstring &wstrPath)
{
	wstring wstrResult = standardize_path(wstrPath);
	size_t stIdx = wstrResult.find_last_of('/');

	// if we find a leading folder
	if (stIdx != wstring::npos)
	{
		wstrResult = wstrResult.substr(0, stIdx);
	}
	// by definition, we return an empty string is no directory can be found
	else
	{
		wstrResult.clear();
	}

	return wstrResult;
}

bool mpom::create_user_data_folder(string &strFullPath, const string &strFolderName)
{
	bool bRes = false;

#ifdef WIN32
	char buf[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathA(0,
		CSIDL_APPDATA,
		NULL,
		SHGFP_TYPE_CURRENT,
		buf
		)))
	{
		strFullPath = buf;
		strFullPath += '\\';
		strFullPath += strFolderName;
		bRes = true;
	}
#else
#ifdef __APPLE__
	// the 'Documents' folder makes sense for OSX
	strFullPath = (string) getenv("HOME") + "/Documents/" + strFolderName;
	bRes = true;
#else	// UNIX
	// for generic unix, just make a 'dot' folder in their home directory
	strFullPath = (string) getenv("HOME") + "/." + strFolderName;
	bRes = true;
#endif // UNIX
#endif

	if (bRes)
	{
		bRes = mpo_mkdir(strFullPath.c_str());
	}

	return bRes;
}

bool mpom::create_user_data_folder(wstring &wstrFullPath, const wstring &wstrFolderName)
{
	string strTmp;
	bool bRes = create_user_data_folder(strTmp, mpom::str_conv(wstrFolderName));
	wstrFullPath = mpom::str_conv(strTmp);
	return bRes;
}

void mpom::write_lile32(void *pBuf, unsigned int u)
{
#ifdef __arm__
	// need to do 1 byte at a time because ARM can freak out over memory alignment issues
	// and we have no way of knowing the alignment of the buffer

	uint8_t *p8Buf = (uint8_t *) pBuf;
	*(p8Buf++) = u & 0xFF;
	*(p8Buf++) = (u >> 8) & 0xFF;
	*(p8Buf++) = (u >> 16) & 0xFF;
	*(p8Buf++) = (u >> 24) & 0xFF;
#else
	// TODO : write big endian version
	*((unsigned int *) pBuf) = u;
#endif
}

void mpom::write_lile64(void *pBuf, MPO_UINT64 u)
{
#ifdef __arm__
	// need to do 1 byte at a time because ARM can freak out over memory alignment issues
	// and we have no way of knowing the alignment of the buffer

	uint8_t *p8Buf = (uint8_t *) pBuf;
	*(p8Buf++) = u & 0xFF;
	*(p8Buf++) = (u >> 8) & 0xFF;
	*(p8Buf++) = (u >> 16) & 0xFF;
	*(p8Buf++) = (u >> 24) & 0xFF;
	*(p8Buf++) = (u >> 32) & 0xFF;
	*(p8Buf++) = (u >> 40) & 0xFF;
	*(p8Buf++) = (u >> 48) & 0xFF;
	*(p8Buf++) = (u >> 56) & 0xFF;
#else
	// TODO : write big endian version
	*((MPO_UINT64 *) pBuf) = u;
#endif
}

unsigned int mpom::read_lile32(void *pBuf)
{
#ifdef __arm__
	// need to do 1 byte at a time because ARM can freak out over memory alignment issues
	// and we have no way of knowing the alignment of the buffer

	uint8_t *p8Buf = (uint8_t *) pBuf;
	unsigned int uRes = p8Buf[0] |
		((uint32_t) p8Buf[1] << 8) |
		((uint32_t) p8Buf[2] << 16) |
		((uint32_t) p8Buf[3] << 24);
	return uRes;
#else
	// TODO : write big endian version
	return *((unsigned int *) pBuf);
#endif
}

MPO_UINT64 mpom::read_lile64(void *pBuf)
{
#ifdef __arm__
	uint8_t *p8Buf = (uint8_t *) pBuf;
	MPO_UINT64 uRes = p8Buf[0] |
		((uint64_t) p8Buf[1] << 8) |
		((uint64_t) p8Buf[2] << 16) |
		((uint64_t) p8Buf[3] << 24) |
		((uint64_t) p8Buf[4] << 32) |
		((uint64_t) p8Buf[5] << 40) |
		((uint64_t) p8Buf[6] << 48) |
		((uint64_t) p8Buf[7] << 56);
	return uRes;
#else
	// TODO : write big endian version
	return *((MPO_UINT64 *) pBuf);
#endif
}

// endian neutral functions (favors big endian)

string mpom::uint2bige32(unsigned int u)
{
	return uint2bige(u);
}

string mpom::uint2bige64(MPO_UINT64 u)
{
	return uint2bige(u);
}

unsigned int mpom::bige2uint32(const void *buf)
{
	unsigned int u32Result = 0;
	bige2uint(buf, u32Result);
	return u32Result;
}

MPO_UINT64 mpom::bige2uint64(const void *buf)
{
	MPO_UINT64 u64Result = 0;
	bige2uint(buf, u64Result);
	return u64Result;
}

//////////////////////////////////////////////////////

mpo_buf::mpo_buf()
{
	m_uSize = 0;
	m_ptr = NULL;
}

mpo_buf::~mpo_buf()
{
	dealloc();
}

// copy constructor
mpo_buf::mpo_buf(const mpo_buf &src)
{
	copy(src.m_ptr, src.m_uSize);
}

mpo_buf& mpo_buf::operator=(const mpo_buf &src)
{
	if (this != &src)
	{
		copy(src.m_ptr, src.m_uSize);
	}
	return *this;
}

mpo_buf& mpo_buf::operator=(const string &strSrc)
{
	copy(strSrc.data(), (unsigned int) strSrc.size());
	return *this;
}

mpo_buf& mpo_buf::operator=(const char *cpszSrc)
{
	copy(cpszSrc, (unsigned int) strlen(cpszSrc));
	return *this;
}

// equal operator
bool mpo_buf::operator==(const mpo_buf &src) const
{
	bool bResult = false;

	// first do the cheap size check
	if (src.size() == m_uSize)
	{
		// if sizes are equal, do a full memcmp
		if (memcmp(src.data_const(), m_ptr, m_uSize) == 0) bResult = true;
	}
	return bResult;
}

// not-equal operator
bool mpo_buf::operator!=(const mpo_buf &src) const
{
	return !(src == *this);
}

void mpo_buf::copy(const void *src, unsigned int uSize)
{
	dealloc();
	m_uSize = uSize;
	m_ptr = new unsigned char [m_uSize];
	if (m_ptr)
	{
		memcpy(m_ptr, src, m_uSize);
	}
}

bool mpo_buf::alloc(unsigned int uSize)
{
	bool bResult = false;
	dealloc();
	m_uSize = uSize;
	m_ptr = new unsigned char[m_uSize];
	if (m_ptr) bResult = true;
	return bResult;
}

unsigned char *mpo_buf::data()
{
	return m_ptr;
}

const unsigned char *mpo_buf::data_const() const
{
	return (const unsigned char *) m_ptr;
}

unsigned int mpo_buf::size() const
{
	return m_uSize;
}

void mpo_buf::dealloc()
{
	if (m_ptr != NULL)
	{
		delete [] m_ptr;
		m_ptr = NULL;
	}
}

////////////////////////////////////////////////

#ifdef HAVE_ZLIB_H

mpo_unzip::mpo_unzip()
{
	m_opened_file = NULL;
}

mpo_unzip::~mpo_unzip()
{
	close();
}

bool mpo_unzip::open(const string &strZipFile)
{
	bool bResult = false;

	close();	// make sure any previously open file is closed
	m_opened_file = unzOpen(strZipFile.c_str());
	if (m_opened_file) bResult = true;

	return bResult;
}

bool mpo_unzip::read_file(const string &strFile, mpo_buf &buf)
{
	bool bResult = false;

	if (unzLocateFile(m_opened_file, strFile.c_str(), 2) == UNZ_OK)
	{
		unz_file_info info;
		unzGetCurrentFileInfo (m_opened_file,
					     &info,
					     NULL,
					     0,
					     NULL,
					     0,
					     NULL,
					     0);

		// allocate space to hold file
		if (buf.alloc(info.uncompressed_size))
		{
			// try to open the current file that we've located
			if (unzOpenCurrentFile(m_opened_file) == UNZ_OK)
			{
				// read this file
				unsigned int bytes_read = unzReadCurrentFile(m_opened_file, buf.data(), buf.size());
				if (bytes_read == buf.size())
				{
					bResult = true;
				}
				unzCloseCurrentFile(m_opened_file);
			}
		}
	}

	return bResult;
}

bool mpo_unzip::test_all(void (*notify)(void *unknown, unsigned int uPos, unsigned int uMaxPos))
{
	bool bValid = true;
	int i = 0;
	mpo_buf buf;

	buf.alloc(100000);	// this number can be of arbitrary size, the bigger the faster, but more memory required

	i = unzGoToFirstFile(m_opened_file);
	while (i == UNZ_OK)
	{
		if (unzOpenCurrentFile(m_opened_file) == UNZ_OK)
		{
			while (unzeof(m_opened_file) != 1)
			{
				int bytes_read = unzReadCurrentFile(m_opened_file, buf.data(), buf.size());
				if (bytes_read < 0)
				{
					bValid = false;
					break;
				}
			}
		}
		i = unzGoToNextFile(m_opened_file);
	}

	// if the while loop did not end the way we expect
	if (i != UNZ_END_OF_LIST_OF_FILE) bValid = false;

	return bValid;
}

bool mpo_unzip::extract_all(const string &strSaveDir, string &strErrMsg, void (*notify)(void *unknown, unsigned int uPos, unsigned int uMaxPos))
{
	bool bValid = true;
	int i = 0;
	mpo_buf bufData, bufFileName;

	strErrMsg = "(no error)";

	// so that we can get write permissions on all the files first before attempting to overwrite them
	list <mpo_io *> lFileHandles;

	mpo_mkdir(strSaveDir.c_str());	// make sure this directory exists
	bufData.alloc(100000);	// this number can be of arbitrary size, the bigger the faster, but more memory required
	bufFileName.alloc(512);	// holds the file name
	char *pszDataPtr = (char *) bufFileName.data();

	// PHASE 1:
	// Acquire write access to all files in question, and create all directories that need to be created.
	// If this phase fails in any way, we won't have done any permanent harm to pre-existing files that we will be overwriting.

	i = unzGoToFirstFile(m_opened_file);
	while ((i == UNZ_OK) && (bValid))
	{
		unz_file_info info;
		unzGetCurrentFileInfo (m_opened_file,
					     &info,
					     pszDataPtr,
					     bufFileName.size(),
					     NULL,
					     0,
					     NULL,
					     0);

		unsigned int uNameLen = (unsigned int) strlen((const char *) pszDataPtr);

		string strFileName = strSaveDir + "/";
		strFileName += pszDataPtr;

		// safety check, make sure we have a filename
		if (uNameLen < 1)
		{
			strErrMsg = "Empty filename inside .zip file";
			bValid = false;
		}

		// directory check: filename must by longer than 1 character (so it can be "d/" for example) and end in '/'
		else if (pszDataPtr[uNameLen-1] == '/')
		{
			// safety check: make sure file name isn't '/'
			if (uNameLen > 1)
			{
				// if directory creation fails ...
				if (!mpo_mkdir(strFileName.c_str()))
				{
					strErrMsg = "Creation of directory " + strFileName + " failed.";
					bValid = false;
				}
			}
			// else if filename is '/' we can probably safely ignore it because it's the root directory which already exists
		}

		// else if it's not a directory, it's a file
		else
		{
#ifdef WIN32
			// make sure the read-only flag is not set
			DWORD dwAttrs = GetFileAttributes(strFileName.c_str()); 

			// if the read-only attribute is set
			if (dwAttrs & FILE_ATTRIBUTE_READONLY)
			{
				// remove the 'read-only' bit
				dwAttrs &= ~FILE_ATTRIBUTE_READONLY;

				// don't necessarily need to check to see whether this succeeds or not because the subsequent mpo_open will know whether we have write access
				SetFileAttributes(strFileName.c_str(), 
					dwAttrs); 
			}
#endif

			// try to get write access to the file (or create it if it does not exist)
			mpo_io *io = mpo_open(strFileName.c_str(), MPO_OPEN_READWRITE);

			// if we've got write access, add it to our list of open handles
			if (io)
			{
				lFileHandles.push_back(io);
			}

			// we were unable to obtain write access, so eject!
			else
			{
				strErrMsg = "Unable to obtain write access to " + strFileName;
				bValid = false;
			}
		}
		i = unzGoToNextFile(m_opened_file);
	} // end while

	// PHASE 2:
	// Now that we have obtained write access to all files, and created all directories without errors (unless bvalid is false of course),
	//  erase each open file (via truncation) and write to it.

	// iterator to our open file handles
	list<mpo_io *>::iterator li = lFileHandles.begin();	

	i = unzGoToFirstFile(m_opened_file);
	while ((i == UNZ_OK) && (bValid))
	{
		unz_file_info info;
		unzGetCurrentFileInfo (m_opened_file,
					     &info,
					     pszDataPtr,
					     bufFileName.size(),
					     NULL,
					     0,
					     NULL,
					     0);

#ifdef DEBUG
		cout << "Phase 2: Filename is " << pszDataPtr << endl;	// DEBUG, REMOVE ME
#endif
		unsigned int uNameLen = (unsigned int) strlen((const char *) pszDataPtr);

		string strFileName = strSaveDir + "/";
		strFileName += pszDataPtr;

		// don't need to do safety check because we already did it in phase 1

		// is it a filename instead of a directory?
		if (pszDataPtr[uNameLen-1] != '/')
		{
#ifdef DEBUG
			// this should always be true
			assert(li != lFileHandles.end());
#endif
			// if we can access the file inside the .zip archive AND if we can erase the currently open corresponding file
			if ((unzOpenCurrentFile(m_opened_file) == UNZ_OK) && mpo_truncate(0, *li))
			{
				// read in the file until we hit EOF
				while (unzeof(m_opened_file) != 1)
				{
					int bytes_read = unzReadCurrentFile(m_opened_file, bufData.data(), bufData.size());
					// if we read something
					if (bytes_read > 0)
					{
						// if we can't write for some reason
						if (!mpo_write(bufData.data(), bytes_read, NULL, *li))
						{
							strErrMsg = "Unable to write to file " + strFileName;
							bValid = false;
						}
					}
					// else if we got an error
					else if (bytes_read < 0)
					{
						strErrMsg = "Error reading from .zip file. It may be corrupt.";
						bValid = false;
					}
					// else EOF or decompression overhead, so ignore
				}
			}
			// else file cannot be accessed or target file cannot be erased
			else
			{
				strErrMsg = "Unable to open file within .zip file, or else cannot truncate open target file.";
				bValid = false;
			}

			// we've just dealt with the current open file handle, so we need to advance to the next one
			++li;
		}
		// else we're dealing with a directory, so we can ignore

		i = unzGoToNextFile(m_opened_file);
	}

	// now close all open file handles, regardless of whether we pass or fail
	for (li = lFileHandles.begin(); li != lFileHandles.end(); li++)
	{
		mpo_close(*li);
	}

	// if the while loop did not end the way we expect
	if ((i != UNZ_END_OF_LIST_OF_FILE) && (bValid))
	{
		strErrMsg = "Unknown error while traversing .zip file";
		bValid = false;
	}

	return bValid;
}

void mpo_unzip::close()
{
	if (m_opened_file)
	{
		unzClose(m_opened_file);
		m_opened_file = NULL;
	}
}

#endif // whether zlib is present

//////////////////

bool spawn_executable(list <string> cmd_line, MPO_PID &pid)
{
	bool result = false;

	// windows and unix have different methods for launching a new process
#ifdef WIN32
	// windows wants the command line in one big string
	string cmd_s = "";
	// add command line to one big line win32 string ...
	for (list<string>::const_iterator li = cmd_line.begin(); li != cmd_line.end(); li++)
	{
		cmd_s += escape_arg(*li);
		cmd_s += " ";
	}

	size_t stArraySize = cmd_s.size() + 1;	// + 1 for null terminator
	char *cmd_line_buf = new char[stArraySize];	// because createprocess won't accept const char *
    SHARED_ARRAY(char) cleanerupper(cmd_line_buf); // this will free up the memory automatically

	memcpy(cmd_line_buf, cmd_s.c_str(), stArraySize);	// this will also copy null terminator over

	PROCESS_INFORMATION process_info;
	STARTUPINFO startup_info;
	memset(&startup_info, 0, sizeof(startup_info));
	startup_info.cb = sizeof(startup_info);
	BOOL b_result = CreateProcess(cmd_line.begin()->c_str(),
		cmd_line_buf, 
		NULL,	// process attributes
		NULL,	// thread attributes
		FALSE,	// inherit handles?
		0,	// creation flags
		NULL,	// new environment
		NULL,	// current directory (same as us)
		&startup_info,	// startup info
		&process_info);	// process information
	if (b_result != 0) result = true;

	pid = process_info.hProcess;
#else
	const char **argv = (const char **) malloc(sizeof (const char *) * (cmd_line.size() + 1));
	// allocate memory to store command line, add an extra slot for NULL termination

	unsigned int i = 0;

	// add command line to one big line win32 string ...
	for (list<string>::const_iterator li = cmd_line.begin(); li != cmd_line.end(); li++)
	{
		argv[i] = li->c_str();
		++i;
	}
	argv[i] = NULL;	// final null terminating entry

	// fork and exec
	pid = fork();

	// if we are the child process
	if (pid == 0)
	{
		// If exec failed ...
		// NOTE : we want this to be execvp so it uses the PATH to search for executables
		if (execvp(argv[0], (char* const*) argv) < 0)
		{
			fprintf(stderr, "argv[0] is %s\n", argv[0]);
			perror("Execvp failed, error is");
			exit(1);	// critical shutdown ...
		}
	}

	// parent process
	else
	{
		result = true;	// FIXME : we always succeed for now, but is that really what we want to do?
	}	

	free(argv);
#endif

	return result;
}

waitr wait_executable(MPO_PID pid, int *exit_code, unsigned int uTimeoutMs)
{
	waitr result = WAITR_BUSY;
#ifdef WIN32
	DWORD wait_result = WAIT_TIMEOUT;

	wait_result = WaitForSingleObject(pid, uTimeoutMs);

	// if we've finished waiting for process ...
	if (wait_result != WAIT_TIMEOUT)
	{
		result = WAITR_FINISHED;

		// if the exit code has been requested
		if (exit_code != NULL)
		{
			DWORD tmp = 0;
			GetExitCodeProcess(pid, &tmp);
			*exit_code = tmp;	// may lose precision here, but exit codes are usually small
		}
		CloseHandle(pid);	// not sure if this is necessary, but some example code shows it
	}
#else
	int status = 0;
	pid_t p = waitpid(pid, &status, WNOHANG);	// check to see if child has exited, but don't wait
	
	// some error
	if (p == -1)
	{
		result = WAITR_ERROR;
	}
	// else if the child we were waiting for is done, we're done too
	else if (p == pid)
	{
		if (exit_code != NULL)
		{
			// if child exited properly (without segfaulting, for example)
			if (WIFEXITED(status) == true)
			{
				*exit_code = WEXITSTATUS(status);
			}
			else *exit_code = -1;	// this will be our generic error for improper termination
		}
		result = WAITR_FINISHED;
	}
	// else p is either 0 ('timed out') or an unknown
	//  so we treat it as a timeout.
#endif
	return result;
}

string escape_arg(const string &arg)
{
	string strResult = "";
	bool bFoundSpace = false;

	for (string::const_iterator si = arg.begin(); si != arg.end(); si++)
	{
		char ch = *si;
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
