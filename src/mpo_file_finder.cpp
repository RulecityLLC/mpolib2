#include <mpolib2/mpo_file_finder.h>
#include "mpo_file_finder_internal.h"
//#include <mpolib2/mpo_misc.h>	// for string conversion
#include <assert.h>
#include <stdexcept>


#ifndef WIN32
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#endif

MpoFindResultSPtr MpoFindResult::GetInstance(const wstring &wstrName, bool bIsDirectory)
{
	MpoFindResultSPtr pRes;
	MpoFindResult *pInstance = new MpoFindResult();
	pInstance->m_wstrName = wstrName;
	pInstance->m_bIsDirectory = bIsDirectory;
	pRes = MpoFindResultSPtr(pInstance, MpoFindResult::deleter());
	return pRes;
}

wstring MpoFindResult::GetName() const
{
	return m_wstrName;
}

bool MpoFindResult::IsDirectory() const
{
	return m_bIsDirectory;
}

MpoFindResult::MpoFindResult()
{
}

////////////////////

IMpoFileFinderSPtr MpoFileFinderFactory::CreateInstance()
{
    return MpoFileFinder::GetInstance();
}

IMpoFileFinderSPtr MpoFileFinder::GetInstance()
{
	return IMpoFileFinderSPtr(new MpoFileFinder(), MpoFileFinder::deleter());
}

MpoFindResultSPtr MpoFileFinder::StartFileFind(const wstring &wstrDirName)
{
	bool bRes = false;
	MpoFindResultSPtr result;

	// if they haven't finished a previous find, then shutdown for them
	if (m_bFindStarted)
	{
		Shutdown();
	}

#ifdef WIN32
	wstring wstrFileMask = wstrDirName + L"\\*.*";
	m_FindHandle = FindFirstFileW(wstrFileMask.c_str(), &m_FindData);

	bRes = (m_FindHandle != INVALID_HANDLE_VALUE);
	if (bRes)
	{
		m_bFindStarted = true;
		result = MpoFindResult::GetInstance(m_FindData.cFileName, (m_FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

	}
#else
	try
	{
		// on unix, the convention is UTF8 for file names (tested on OSX and linux)
		m_strDirName = mpom::ToUTF8Ex(wstrDirName);
		m_dir = opendir(m_strDirName.c_str());
		bRes = (m_dir != NULL);
	}
	catch (std::exception &ex)
	{
		bRes = false;
	}
	
	// We need to return our first result immediately according to this function's definition, so we just use the NextFileFind function to do it.
	if (bRes)
	{
		m_bFindStarted = true;
		result = NextFileFind();
	}
	
#endif // WIN32

	// Throw away "." and ".." (windows returns these, other OS's may).
	// The reason I decided to throw them away is because they are implicit and redundant because we require a directory name to be passed in.
	while (result.get() && ((result->GetName() == L".") || (result->GetName() == L"..")))
	{
		result = NextFileFind();
	}

	if (!bRes)
	{
		result.reset();
	}

	return result;
}

MpoFindResultSPtr MpoFileFinder::NextFileFind()
{
	bool bRes = false;
	MpoFindResultSPtr result;

	if (!m_bFindStarted) return result;

	// Throw away "." and ".." (windows returns these, so does linux, other OS's may).
	// The reason I decided to throw them away is because they are implicit and redundant because we require a directory name to be passed in.
	do
	{
#ifdef WIN32
		BOOL b = FindNextFileW(m_FindHandle, &m_FindData);
		bRes = (b != FALSE);
		if (bRes)
		{
			result = MpoFindResult::GetInstance(m_FindData.cFileName, (m_FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
		}
#else
		struct dirent *stFiles = readdir(m_dir);
		bRes = (stFiles != NULL);
		if (bRes)
		{
			string strPath = m_strDirName + "/" + stFiles->d_name;
			struct stat stFileInfo;
			if (stat(strPath.c_str(), &stFileInfo) == 0)
			{
				// on unix, the convention is UTF8 for filenames (tested on OSX and linux)
				result = MpoFindResult::GetInstance(mpom::FromUTF8Ex(stFiles->d_name), S_ISDIR(stFileInfo.st_mode));
			}
			// else stat failed (shouldn't happen)
			else
			{
				string strMsg = "stat failed with error: ";
				char buf[80];
				strMsg += strerror_r(errno, buf, sizeof(buf));
				throw runtime_error(strMsg);
			}
		}
#endif // WIN32
	} while (bRes && ((result->GetName() == L".") || (result->GetName() == L"..")));

	if (!bRes)
	{
		result.reset();
	}

	return result;
}

MpoFileFinder::MpoFileFinder() :
m_bFindStarted(false)
{
}

MpoFileFinder::~MpoFileFinder()
{
	Shutdown();
}

void MpoFileFinder::Shutdown()
{
	if (m_bFindStarted)
	{
#ifdef WIN32
		FindClose(m_FindHandle);
#else
#ifndef NDEBUG
		int i =
#endif
		closedir(m_dir);
		assert(i == 0);
#endif
	}
	m_bFindStarted = false;
}
