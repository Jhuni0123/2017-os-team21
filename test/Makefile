CC = arm-linux-gnueabi-gcc

all : file_loc gpsupdate

file_loc : file_loc.c
	$(CC) -o file_loc file_loc.c

gpsupdate : gpsupdate.c
	$(CC) -o gpsupdate gpsupdate.c

test :
	$(CC) -o getfileloc getfileloc.c
	$(CC) -o writefile writefile.c
	$(CC) -o readfile readfile.c
	$(CC) -o sleep sleep.c
	
clean :
	rm -f *.o file_loc gpsupdate getfileloc writefile readfile sleep

