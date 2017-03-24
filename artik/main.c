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
	int i, tab = 0;
	int prev_pid = -1;
	int prev_parent = (struct prinfo*)buf->parent_pid;
	for(i = 0; i < result; i++) {
		struct prinfo* p = (struct prinfo*)buf + i;
		if(prev_parent != p->parent_pid)
			if(prev_pid == p->parent_pid)
				tab++;
			else
				tab--;
		int j;
		char line[100];
		line[0] = '\0';
		for(j = 0; j < 2*tab; j++)
			strcat(line, "  ");
		strcat(line, "%s,%d,%ld,%d,%d,%d,%d\n");
		printf(line, p->comm, p->pid, p->state,
			p->parent_pid, p->first_child_pid, p->next_sibling_pid, p->uid);
		
		prev_parent = p->parent_pid;
		prev_pid = p->pid;
	}

	//return test(); //uncomment this to run test
	return 0;
}
