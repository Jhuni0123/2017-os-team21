CC = arm-linux-gnueabi-gcc

all: multiproc plot

multiproc: multiproc.c
	$(CC) -o multiproc multiproc.c

plot: plot.c
	$(CC) -o plot plot.c

clean:
	rm -f multiproc plot
