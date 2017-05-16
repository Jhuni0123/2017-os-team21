#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include "trial.h"

#define FACTORIZENUMBER 1229*1847
#define NTRIAL 10

double record[NTRIAL];

int main(int argc, char** argv)
{
	int i, j;
	
	if(syscall(380, getpid(), 1) < 0)		
		printf("setweight error.\n");
	pid_t pid = getpid();
	// run once for caching
	init(FACTORIZENUMBER);
	primefactor(FACTORIZENUMBER);		
	clean();
	double sum = 0.0;
	for(j = 1; j <= NTRIAL; j++)
	{
			struct timeval start, end;
			init(FACTORIZENUMBER);
			usleep(10);

			gettimeofday(&start, NULL);
			primefactor(FACTORIZENUMBER);		
			gettimeofday(&end, NULL);
			
			double sec = ((end.tv_sec  - start.tv_sec) * 1000000u +
					 end.tv_usec - start.tv_usec) / 1.e6;
			record[j-1] = sec;
			clean();
			sum += sec;
			if(argc == 1)
				printf("Trial %d:\t%lfs\n", j, sec);
		
	}
	printf("Averge:\t%lfs\n",  sum / NTRIAL);
}
