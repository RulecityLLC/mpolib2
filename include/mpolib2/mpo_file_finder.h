#ifndef MPO_FILE_FINDER_H
#define MPO_FILE_FINDER_H

#include "mpo_deleter.h"
#include "mpo_dll.h"

#include <string>
using std::string;
using std::wstring;

class IMpoFindResult
{
public:
	virtual wstring GetName() const = 0;

	virtual bool IsDirectory() const = 0;
};

typedef shared_ptr<IMpoFindResult> MpoFindResultSPtr;

class IMpoFileFinder
{
public:
	virtual MpoFindResultSPtr StartFileFind(const wstring &wstrDirName) = 0;

	virtual MpoFindResultSPtr NextFileFind() = 0;
};

typedef shared_ptr<IMpoFileFinder> IMpoFileFinderSPtr;

class EXPORT_ME MpoFileFinderFactory {
public:
    static IMpoFileFinderSPtr CreateInstance();
};

#endif // MPO_FILE_FINDER_H
