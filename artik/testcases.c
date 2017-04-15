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

#define THREAD(i, x) pthread_create(&pthread[i], NULL, x, NULL)

#define F(str) { printf(str"\n"); return -1; }

int c = 0;

int v1, v2, v3;
void t1(void *d)
{
	SUC(LOCK_WRITE, 60, 30);
	v1 = 1;
	SUC(UNLOCK_WRITE, 60, 30);
}

int main()
{
	// test cases referenced by issue #68
	// https://github.com/swsnu/osspr2017/issues/68
	pthread_t pthread[20];

	NEXT_CASE;
	
	// failed lock
	SUC(SET_ROTATION, 0, 0);
	SUC(LOCK_WRITE, 30, 30);
	FAIL(UNLOCK_WRITE, 29, 30);
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


}

