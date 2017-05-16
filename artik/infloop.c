#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>

#define NLOOP 16

int main()
{
	int i;

	for(i = 0; i < NLOOP; i++)
	{
		if(fork() == 0)
		{
			syscall(380, getpid(), 20);
			while(1) ;
		}
	}
}
