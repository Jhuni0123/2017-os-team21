#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdlib.h>
#include "prinfo.h"

#define FAIL(str) {printf("FAIL: "str"\n"); all_success = 0;}
int test()
{
	int all_success = 1;
	
	int nr = 500;
	void* buf = malloc(sizeof(struct prinfo) * nr);
	
	int small_nr = 2;
	void* small_buf = malloc(sizeof(struct prinfo) * small_nr);
	int negative_nr = -1;
	int zero_nr = 0;
	
	// will result test() corruption if not handled well
	int (*test_ptr) () = test;
	syscall(380, test_ptr, &nr);
	
	if(syscall(380, buf, &nr) < 0)
		FAIL("valid function call");
	if(syscall(380, buf) >= 0)
		FAIL("invalid parameter number, small chance to give valid nr");
	if(syscall(380, buf, &nr, buf) < 0)
		FAIL("valid function call with 3 parameters");
	if(syscall(380, NULL, &nr) >= 0 || errno != EINVAL)
		FAIL("null buf");
	if(syscall(380, (unsigned)-10000, &nr) >= 0 || errno != EFAULT)
		FAIL("invalid buf location");
	if(syscall(380, buf, NULL) >= 0 || errno != EINVAL)
		FAIL("null nr");
	if(syscall(380, buf, (unsigned)-10000) >= 0 || errno != EFAULT)
		FAIL("invalid nr location");
	
	if(syscall(380, small_buf, &small_nr) < 0)
		FAIL("valid function call with small parameter");
	if(syscall(380, small_buf, &nr) >= 0 || errno != EFAULT)
		FAIL("small buf & large nr");
	if(syscall(380, buf, &negative_nr) >= 0 || errno != EINVAL)
		FAIL("negative nr");
	if(syscall(380, buf, &zero_nr) < 0)
		FAIL("valid function call with nr=0");	
	
	syscall(380, buf, &small_nr);
	if(*(int*)((struct prinfo*)buf + small_nr + 1) != 0)
		FAIL("copied more than nr");
	
	if(all_success)
		return 0;
	else
		return -1;
}
