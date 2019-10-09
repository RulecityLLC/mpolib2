#include "test_headers.h"
#include <mpolib2/mpo_file_finder.h>

void test_filefind1()
{
	set<wstring> setResults;	// to sort results for consistency (the directories are enumerated in a different order on windows and linux)
	IMpoFileFinderSPtr FinderSPtr = MpoFileFinderFactory::CreateInstance();
	IMpoFileFinder *pFinder = FinderSPtr.get();
	TEST_REQUIRE(pFinder != 0);

	IMpoFileIOSPtr fileIO = MpoFileIOFactory::CreateInstance();
	IMpoFileIO *pFileIO = fileIO.get();

	pFileIO->MkDir(L"mydir");

	blocking_sharedptr FileSPtr = MpoFileStreamFactory::CreateInstance("mydir/nukeme.bin", MPO_OPEN_CREATE);
	IBlockingStream *pStream = FileSPtr.get();

	TEST_REQUIRE(pStream != 0);

	char ch = 'A';
	size_t stRes = pStream->Write(&ch, sizeof(ch));
	TEST_REQUIRE_EQUAL(1, stRes);

	FileSPtr.reset();	// close file

	FileSPtr = MpoFileStreamFactory::CreateInstance("mydir/nukeme2.bin", MPO_OPEN_CREATE);
	pStream = FileSPtr.get();
	TEST_REQUIRE(pStream != 0);
	FileSPtr.reset();	// close file

	MpoFindResultSPtr res;

	// try to call NextFileFind before calling FirstFileFind
	res = pFinder->NextFileFind();
	TEST_REQUIRE(!res.get());

	// try to find the file we just created
	res = pFinder->StartFileFind(L"mydir");
	TEST_REQUIRE(res.get());

	wstring wstrName = res->GetName();
	setResults.insert(wstrName);

	res = pFinder->NextFileFind();
	TEST_CHECK(res.get());

	wstrName = res->GetName();
	setResults.insert(wstrName);

	set<wstring>::const_iterator si = setResults.begin();
	TEST_CHECK(L"nukeme.bin" == *si);
	si++;
	TEST_CHECK(L"nukeme2.bin" == *si);

	res = pFinder->NextFileFind();
	TEST_CHECK(!res.get());

	// partial clean-up
	FinderSPtr.reset();	// close findfile object (so directory can be deleted)
	pFileIO->Delete(L"mydir/nukeme.bin");
	pFileIO->Delete(L"mydir/nukeme2.bin");

	// make sure we can handle empty directories
	FinderSPtr = MpoFileFinderFactory::CreateInstance();
	pFinder = FinderSPtr.get();
	TEST_REQUIRE(pFinder != 0);
	res = pFinder->StartFileFind(L"mydir");
	TEST_CHECK(!res.get());
	FinderSPtr.reset();	// close findfile object (so directory can be deleted)

	// final clean-up
	pFileIO->RmDir(L"mydir");
}

TEST_CASE(filefind1)
{
	test_filefind1();
}
