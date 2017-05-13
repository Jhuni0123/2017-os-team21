#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>


int* sieve;	// 0-initialized
int* prime;
int prime_size = 0;

void factorize(int num)
{
    for(int i = 0; prime[i] * prime[i] <= num; i++) {
        while(num % prime[i] == 0 && prime[i] < num) {
            num /= prime[i];
        }
    }
}

void primefactor(int num)
{
    for(int i = 2; i <= num; i++) {
        if(sieve[i] == 0) {
            prime[prime_size++] = i;
            for(int j = i*2; j <= num; j += i) {
                sieve[j] = 1;
            }
        }
    }
    
    factorize(num);
}

void init(int num)
{
	sieve = (int*)calloc((num+1), sizeof(int));
	prime = (int*)malloc(sizeof(int) * (num/10));
	prime_size = 0;
}

void clean()
{
	free(sieve);
	free(prime);
}
