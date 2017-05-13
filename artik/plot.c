#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include "trial.h"

#define FACTORIZENUMBER 1229*1847*11
#define NTRIAL 3

double record[20][NTRIAL];

int main(int argc, char** argv)
{
	int i, j;
		
	pid_t pid = getpid();
	// run once for caching
	init(FACTORIZENUMBER);
	primefactor(FACTORIZENUMBER);		
	clean();
	for(j = 1; j <= NTRIAL; j++)
	{
		if(argc == 1)
			printf("\nTrial #%d:\n", j);
		for(i = 1; i <= 20; i++)
		{
			struct timeval start, end;
			if(syscall(380, pid, i) < 0)		
				printf("setweight error.\n");
			init(FACTORIZENUMBER);
			usleep(10);

			gettimeofday(&start, NULL);
			primefactor(FACTORIZENUMBER);		
			gettimeofday(&end, NULL);
			
			double sec = ((end.tv_sec  - start.tv_sec) * 1000000u +
					 end.tv_usec - start.tv_usec) / 1.e6;
			record[i-1][j-1] = sec;
			clean();
			if(argc == 1)
				printf("Weight %d:\t%lfs\n", i, sec);
		}
	}
	
	if(argc == 1)
	{
		printf("\n===Average===\n");
		for(i = 0; i < 20; i++)
		{
			double sum = 0;
			for(j = 0; j < NTRIAL; j++)
				sum += record[i][j];
			printf("Weight %d:\t%lf\n", i+1, sum/(double)NTRIAL);
		}
		printf("put any arguments to executable to print CSV part only.\n");
		printf("=============\n\n");
	}

	for(i = 0; i < 20; i++)
	{
		double sum = 0;
		for(j = 0; j < NTRIAL; j++)
		{
			printf("%lf,", record[i][j]);
			sum += record[i][j];
		}
		printf("%lf\n", sum/(double)NTRIAL);
	}
	return 0;
}
