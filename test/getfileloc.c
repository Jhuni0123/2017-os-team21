#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include "gps.h"

#define GET_GPS_LOCATION 381

int main(int argc, char** argv)
{
	if(argc < 2){
		printf("Invalid input\n");
		return 1;
	}

	struct gps_location loc;

	if(syscall(GET_GPS_LOCATION, argv[1], &loc)){
		return 1;
	}

	printf("%d.%d %d.%d %d", loc.lat_integer, loc.lat_fractional,
		loc.lng_integer, loc.lng_fractional, loc.accuracy);

	return 0;
}
