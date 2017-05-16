CC = arm-linux-gnueabi-gcc

all: multiproc plot aging

multiproc: multiproc.c
	$(CC) -o multiproc multiproc.c

plot: plot.c
	$(CC) -o plot plot.c

aging: ageplot.c infloop.c
	$(CC) -o ageplot ageplot.c
	$(CC) -o infloop infloop.c

clean:
	rm -f multiproc plot ageplot infloop
