#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define ROTLOCK_WRITE 382
#define ROTUNLOCK_WRITE 384


int main(int argc, char *argv[])
{
    if(argc < 2) {
        printf("Error: There should be an integer argument\n");
        return 1;
    }
    
    int num = atoi(argv[1]);

    while(1) {
        syscall(ROTLOCK_WRITE, 90, 90);

        FILE *integer = fopen("integer", "w");
        fprintf(integer, "%d", num);
        fclose(integer);
        printf("selector: %d\n", num);
        num++;
        //sleep(1);

        syscall(ROTUNLOCK_WRITE, 90, 90);
    }

}
