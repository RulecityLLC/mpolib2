/*
 * mpo_fileio.h
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

// mpo_fileio.h
// by Matt Ownby

#ifndef MPO_FILEIO_H
#define MPO_FILEIO_H

#include "mpo_types.h"
#include "mpo_deleter.h"
#include "mpo_dll.h"
#include <string>

using std::string;
using std::wstring;

// General purpose file I/O class that serves as a nice replacement to calling mpo_fileio C-styled functions directly
// The purpose of this class is so that this class can easily be mocked out for unit testing.
// Wide file names are used exclusively to encourage migration in that direction. :)
class IMpoFileIO
{
public:
    virtual bool FileExists(const wstring &wstrFileExists) = 0;

    virtual void MkDir(const wstring &wstrDirName) = 0;

    virtual void RmDir(const wstring &wstrDirName) = 0;

    virtual MPO_UINT64 GetFreeBytes(const wstring &wstrDirName) = 0;

    virtual void Delete(const wstring &wstrFileName) = 0;
};

typedef shared_ptr<IMpoFileIO> IMpoFileIOSPtr;

class EXPORT_ME MpoFileIOFactory
{
public:
    static IMpoFileIOSPtr CreateInstance();
};

#endif // MPO_FILEIO_H
