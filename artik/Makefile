CC = arm-linux-gnueabi-gcc
CCTEST = gcc

all : selector trial test

selector :
	$(CC) -o selector selector.c

trial :
	$(CC) -o trial trial.c

test :
	$(CC) -o test test.c

local :
	$(CCTEST) -o selector selector.c
	$(CCTEST) -o trial trial.c
	$(CCTEST) -o test test.c

clean :
	rm -f *.o selector trial test
