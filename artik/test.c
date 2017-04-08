#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define ROTLOCK_READ 381
#define ROTLOCK_WRITE 382
#define ROTUNLOCK_READ 383
#define ROTUNLOCK_WRITE 384


int main()
{
    printf("## Start Test\n");
    
    printf("## Take Read Lock\n");
    syscall(ROTLOCK_READ, 90, 90);

    printf("## Release Read Lock\n");
    syscall(ROTUNLOCK_READ, 90, 90);


    printf("## Take Write Lock\n");
    syscall(ROTLOCK_WRITE, 90, 90);

    printf("## Release Write Lock\n");
    syscall(ROTUNLOCK_WRITE, 90, 90);

}

