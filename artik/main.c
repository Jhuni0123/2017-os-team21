#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main()
{
	syscall(380, NULL, NULL);
	return 0;
}
