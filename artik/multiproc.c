#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <errno.h>


int main()
{
	int pid = getpid();
	int nProc = 10;
	int nSleep = 1E9;
	int i = 1;
	struct sched_param param;
	param.sched_priority = 0;

	printf("Setting scheduler to WRR..\n");
	if(sched_setscheduler(pid, 6, &param) < 0)
	{
		printf("ERRORNO %s\n", strerror(errno));
		printf("sched_setscheduler failed.\n");
		return 1;
	}
	printf("%dth process %d created. policy: %d\n", i, getpid(), sched_getscheduler(pid));

	while(fork() == 0)
	{
		pid = getpid();
		i++;
		
		if(sched_setscheduler(pid, 6, &param) < 0)
		{
			printf("ERRORNO %s\n", strerror(errno));
			printf("sched_setscheduler failed.\n");
			return 1;
		}
		printf("%dth process %d created. policy: %d\n", i, pid, sched_getscheduler(getpid()));
		if(i == nProc)
			break;
	}
	
	int cnt = 0;
	while(cnt < nSleep)
	{
		cnt++;
		if(cnt % 100000000 == 0)
			printf("%dth process %d woken: %d\n", i, pid, cnt / 100000000);
	}

}
