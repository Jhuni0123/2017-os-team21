CC = arm-linux-gnueabi-gcc
CCTEST = gcc

all : selector trial rotd test

selector :
	$(CC) -o selector selector.c

trial :
	$(CC) -o trial trial.c

rotd :
	$(CC) -o rotd rotd.c

test :
	$(CC) -o test test.c
	$(CC) -o testcases testcases.c -pthread

local :
	$(CCTEST) -o selector selector.c
	$(CCTEST) -o trial trial.c

clean :
	rm -f *.o selector trial rotd test testcases
