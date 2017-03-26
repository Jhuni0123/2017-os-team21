#include <unistd.h>

// This code forks child processes as binary tree
// which means if N=3, there will be 8 leafs

int main()
{
	int i = 0;
	for(i = 0; i < 3; i++)
		if(fork() != 0)
			if(fork() != 0)
				break;
	
	sleep(100);
	return 0;
}
