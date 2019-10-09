#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
#ifdef WIN32
	Sleep(10 * 1000);
//	MessageBoxA(0, "hi", "there", 0);
#else
	sleep(10);
#endif
	return 0;
}
