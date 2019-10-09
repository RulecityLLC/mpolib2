// std_test_exiter.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	// Due to a bug I've been seeing on OSX, I've changed this test to write a block of data to stdout and then exiting,
	//  to simulate the defect.
	void *pBuf = malloc(100000);
	fwrite(pBuf, 1, 100000, stdout);
	fflush(stdout);

	free(pBuf);

	// arbitrary exit code so we know we got the correct exit code
	return 17;
}
