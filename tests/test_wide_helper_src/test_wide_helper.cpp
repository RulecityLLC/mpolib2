// test_wide_helper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <mpolib2/mpo_fileio.h>
#include <mpolib2/mpo_misc.h>

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "Expected argument count is 2, but was %u", argc);
	}

#ifdef WIN32
	// windows' way to get wide command line arguments
	LPWSTR *szArglist;
	int nArgs;
 	int i;

	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	if( NULL == szArglist )
	{
		fprintf(stderr, "CommandLineToArgvW failed\n");
		return 1;
	}

	IMpoFileIOSPtr file = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFile = file.get();
	bool bFileExists = pFile->FileExists(szArglist[1]);

	// Free memory allocated for CommandLineToArgvW arguments.

	LocalFree(szArglist);
#else
	bool bFileExists = mpo_file_exists(argv[1]);
#endif

	// if file exists, then we were able to translate the wide filename correctly
	if (bFileExists)
	{
		return 123;
	}
	return 1;
}
