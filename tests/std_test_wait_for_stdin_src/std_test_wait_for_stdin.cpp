// std_test_wait_for_stdin.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

int main(int argc, char* argv[])
{
	char bufIn[80];

	// this tests the ability for us to shutdown stdin pipe and wait for an exit code

	// read bytes from stdin
	fflush(stdin);
	size_t stBytesRead = 1;

	// read stdin until the pipe is closed
	while (stBytesRead != 0)
	{
		stBytesRead = fread(bufIn, 1, sizeof(bufIn), stdin);
	}
	
	return stBytesRead;
}
