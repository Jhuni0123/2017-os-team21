#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

extern int test();

int main()
{
	syscall(380, NULL, NULL);
	//return test(); //uncomment this to run test
	return 0;
}
