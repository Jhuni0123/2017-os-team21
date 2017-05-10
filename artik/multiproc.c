#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>


int main()
{
	pid_t pid = getpid();
	int nProc = 10;
	int nSleep = 1E9;
	int i = 1;

	printf("Setting scheduler to WRR..\n");
	if(sched_setscheduler(pid, 6, NULL) < 0)
	{
		printf("sched_setscheduler failed.\n");
		return;
	}
	printf("%dth process created. policy: %d\n", i, sched_getscheduler(getpid()));

	while(fork() == 0)
	{
		i++;
		printf("%dth process created. policy: %d\n", i, sched_getscheduler(getpid()));
		if(i == nProc)
			break;
	}
	
	int cnt = 0;
	while(cnt < nSleep)
	{
		cnt++;
		if(cnt % 1E8 == 0)
			printf("%dth process woken: %d\n", i, cnt / 1E8);
	}

}
