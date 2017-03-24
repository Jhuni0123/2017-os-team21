#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include "prinfo.h"


extern int test();

int main()
{
	int nr = 500;
	void* buf = malloc(sizeof(struct prinfo) * nr);
	printf("== Call ptree ==\n");
	int result = syscall(380, buf, &nr);
	
	// print syscall result
	int i;
	int stack[500] = {0};
	int top = 1;
	for(i = 0; i < nr; i++) {
		struct prinfo* p = (struct prinfo*)buf + i;
		while(p->parent_pid != stack[top-1])top--;
			stack[top++] = p->pid;
		int j;
		char line[2000];
		line[0] = '\0';
		for(j = 0; j < 2*(top-2); j++)
			strcat(line, "  ");
		strcat(line, "%s,%d,%ld,%d,%d,%d,%d\n");
		printf(line, p->comm, p->pid, p->state,
			p->parent_pid, p->first_child_pid, p->next_sibling_pid, p->uid);
	}
	printf("Total process:  %d\n", result);
	printf("Copied process: %d\n", nr);
	return test(); //uncomment this to run test
	//return 0;
}
