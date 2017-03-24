#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include "prinfo.h"

extern int test();

int main()
{
	int nr = 200;
	void* buf = malloc(sizeof(struct prinfo) * nr);
	
	int result = syscall(380, buf, &nr);
	
	// print syscall result
	int i, prev_pid = 0, tab = 0;
	int stack[20];
	stack[0] = 0;
	int* sp = stack;
	for(i = 0; i < result; i++) {
		struct prinfo* p = (struct prinfo*)buf + i;
		if(*sp != p->parent_pid) {
			if(prev_pid == p->parent_pid) {
				tab++;
				sp++;
				*sp = p->parent_pid;	
			}
			else {
				tab--;
				sp--;
			}
		}
		int j;
		char line[100];
		line[0] = '\0';
		for(j = 0; j < 2*tab; j++)
			strcat(line, "  ");
		strcat(line, "%s,%d,%ld,%d,%d,%d,%d\n");
		printf(line, p->comm, p->pid, p->state,
			p->parent_pid, p->first_child_pid, p->next_sibling_pid, p->uid);
		prev_pid = p->parent_pid;
	}

	//return test(); //uncomment this to run test
	return 0;
}
