#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <pthread.h>

#define SET_ROTATION 380
#define LOCK_READ 381
#define LOCK_WRITE 382
#define UNLOCK_READ 383
#define UNLOCK_WRITE 385

#define SUC(num, deg, ran) if(syscall(num, deg, ran) < 0) \
{ printf("no successful return in #%d\n", c); return -1; }

#define FAIL(num, deg, ran) if(syscall(num, deg, ran) >= 0) \
{ printf("no fail return in #%d\n", c); return -1; }

#define NEXT_CASE c++; printf("Case %d\n", c);

#define THREAD(i, x) { pthread_create(&pthread[i], NULL, x, NULL); pthread_detach(pthread[i]); }

#define PROC(x) { if(fork == 0) { x(NULL); return 0; } }

#define F(str) { printf(str"\n"); return -1; }

int c = 0;

int v1, v2, v3;
void t1(void *d)
{
	SUC(LOCK_WRITE, 60, 30);
	v1 = 1;
	SUC(UNLOCK_WRITE, 60, 30);
}

void t2(void *d)
{
	SUC(SET_ROTATION, 60, 0);
	SUC(LOCK_WRITE, 30, 30);
	SUC(SET_ROTATION, 120, 0);
	SUC(LOCK_READ, 150, 30);
	sleep(1);
	exit(0);
}

void t3(void *d)
{
	SUC(LOCK_READ, 0, 30);
	v1++;
	sleep(1);
	SUC(UNLOCK_READ, 0, 30);
}

void t4(void* d)
{
	SUC(LOCK_WRITE, 0, 30);
	v2++;
	sleep(1);
	SUC(UNLOCK_WRITE, 0, 30);
}

int main()
{
	// some test cases referenced by issue #68
	// https://github.com/swsnu/osspr2017/issues/68
	pthread_t pthread[20];

	NEXT_CASE;
	
	// failed lock
	SUC(SET_ROTATION, 0, 0);
	SUC(LOCK_WRITE, 30, 30);
	FAIL(UNLOCK_WRITE, 29, 30);
	FAIL(UNLOCK_READ, 30, 30);
	SUC(UNLOCK_WRITE, 30, 30);

	NEXT_CASE;
	
	// read/write functionality
	SUC(LOCK_READ, 0, 60);
	SUC(SET_ROTATION, 60, 0);
	v1 = 0;
	THREAD(0, t1);
	if(v1 == 1)
		F("1");
	SUC(SET_ROTATION, 300, 0);
	SUC(LOCK_READ, 300, 30);
	SUC(SET_ROTATION, 60, 0);
	SUC(UNLOCK_READ, 0, 60);
	sleep(1);
	if(v1 == 0)
		F("2");
	SUC(UNLOCK_READ, 300, 30);
	
	NEXT_CASE;
	
	// 2 threads have same rotation range
	SUC(SET_ROTATION, 60, 0);
	SUC(LOCK_READ, 60, 30);
	THREAD(1, t1);
	sleep(1);
	SUC(UNLOCK_READ, 60, 30);

	NEXT_CASE;

	// process termination case from test case of github issue
	PROC(t2);
	SUC(SET_ROTATION, 60, 0);
	SUC(LOCK_WRITE, 30, 30);
	SUC(SET_ROTATION, 120, 0);
	SUC(LOCK_WRITE, 150, 30);
	SUC(UNLOCK_WRITE, 30, 30);
	SUC(UNLOCK_WRITE, 150, 30);

	NEXT_CASE;
	
	// read/write order
	// RRWRW
	printf("this case will take some time\n");
	v1 = v2 = 0;
	SUC(SET_ROTATION, 180, 0);
	THREAD(2, t3);
	sleep(1);
	THREAD(3, t3);
	sleep(1);
	THREAD(4, t4);
	sleep(1);
	THREAD(5, t3);
	sleep(1);
	THREAD(6, t4);
	SUC(SET_ROTATION, 0, 0);
	while(v2 != 1 || v1 != 0) ;
	while(v2 != 2 || v1 != 0) ;
	while(v2 != 2 || v1 != 3) ;

	printf("all tests passed\n");
}

