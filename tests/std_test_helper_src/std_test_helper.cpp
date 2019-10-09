// std_test_helper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int main(int argc, char* argv[])
{
	char buf1[] = "hi";
	char buf2[] = "there";
	char bufErr[] = "error";
	char bufIn[80];

	// This tests the ability for another process to capture both stdout, stdin, and stderr from a process that it launches.

	// write 2 bytes to stdout
	fwrite(buf1, 1, sizeof(buf1) - 1, stdout);
	fflush(stdout);

	// write to stderr
	fwrite(bufErr, 1, sizeof(bufErr) - 1, stderr);
	fflush(stderr);

	// read 3 bytes from stdin
	fflush(stdin);
	if (fread(bufIn, 1, 3, stdin) != 3)
	{
		return 77;	// this should never happen but it gets rid of compiel warnings
	}

	// write 5 bytes to stdout
	fwrite(buf2, 1, sizeof(buf2) - 1, stdout);
	fflush(stdout);
	
	return 11;	// arbitrary, so we know the process really exit
}
