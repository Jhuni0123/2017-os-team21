#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdlib.h>

#define FAIL(str) {printf("FAIL: "str"\n"); all_success = 0;}
int test()
{
	int all_success = 1;

	void* buf = malloc(10000);
	int nr = 200;
	
	void* small_buf = malloc(100);
	int small_nr = 2;
	int negative_nr = -1;
	int zero_nr = 0;
	
	// will result test() corruption if not handled well
	int (*test_ptr) () = test;
	syscall(380, test_ptr, &nr);
	
	if(syscall(380, buf, &nr) < 0)
		FAIL("valid function call");
	if(syscall(380, buf) >= 0 || syscall(380, buf, &nr, NULL) >= 0)
		FAIL("invalid parameter number");
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
	if(*((int*)buf + 25) != 0)
		FAIL("copied more than nr");
	
	if(all_success)
		return 0;
	else
		return -1;
}
