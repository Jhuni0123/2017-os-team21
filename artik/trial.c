#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

#define PRIME_LIMIT 10000000

#define ROTLOCK_READ 381
#define ROTUNLOCK_READ 383


int sieve[PRIME_LIMIT+1] = {0};
int prime[PRIME_LIMIT/10];
int prime_size = 0;

void factorize(int num)
{
    printf("%d = ", num);
    for(int i = 0; prime[i] * prime[i] <= num; i++) {
        while(num % prime[i] == 0 && prime[i] < num) {
            printf("%d * ", prime[i]);
            num /= prime[i];
        }
    }
    printf("%d\n",num);
}

int main(int argc, char *argv[])
{
    if(argc < 2) {
        printf("Error: There should be an integer argument\n");
        exit(1);
    }

    int id = atoi(argv[1]);

    for(int i = 2; i <= PRIME_LIMIT; i++) {
        if(sieve[i] == 0) {
            prime[prime_size++] = i;
            for(int j = i*2; j <= PRIME_LIMIT; j += i) {
                sieve[j] = 1;
            }
        }
    }
    
    while(1) {
        int num;
        FILE *integer;

        syscall(ROTLOCK_READ, 90, 90);

        integer = fopen("integer", "r");
        fscanf(integer, "%d", &num);

        printf("trial-%d: ", id);
        factorize(num);

        fclose(integer);
        //sleep(1);

        syscall(ROTUNLOCK_READ, 90, 90);
    }
}
