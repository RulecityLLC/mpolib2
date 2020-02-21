#ifndef MPO2_MPO_FILE_FINDER_INTERNAL_H
#define MPO2_MPO_FILE_FINDER_INTERNAL_H

#ifdef WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

class MpoFindResult : public IMpoFindResult, public MpoDeleter
{
public:
static MpoFindResultSPtr GetInstance(const wstring &wstrName, bool bIsDirectory);

wstring GetName() const;

bool IsDirectory() const;
private:
	MpoFindResult();

	virtual ~MpoFindResult() {}

	void DeleteInstance() { delete this; }

wstring m_wstrName;
bool m_bIsDirectory;
};

class MpoFileFinder : public IMpoFileFinder, public MpoDeleter
{
public:
static IMpoFileFinderSPtr GetInstance();

MpoFindResultSPtr StartFileFind(const wstring &wstrDirName);

MpoFindResultSPtr NextFileFind();

private:
	MpoFileFinder();
	virtual ~MpoFileFinder();

	void DeleteInstance() { delete this; }

void Shutdown();

#ifdef WIN32
HANDLE m_FindHandle;
WIN32_FIND_DATAW m_FindData;
#else
	DIR *m_dir;
	string m_strDirName;	// to strip off directory prefix when returning results
#endif // WIN32

bool m_bFindStarted;
};

#endif //MPO2_MPO_FILE_FINDER_INTERNAL_H
