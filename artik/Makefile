CC = arm-linux-gnueabi-gcc

all: clean multiproc plot

multiproc:
	$(CC) -o multiproc multiproc.c

plot:
	$(CC) -o plot plot.c

clean:
	rm -f multiproc plot
