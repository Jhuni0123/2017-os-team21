#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include "gps.h"

#define SET_GPS_LOCATION 380

/* How to use:
 *	./gpsupdate lat_integer lat_fractional lng_integer lng_fractional accuracy 
 * */

int main(int argc, char** argv)
{
	if(argc < 6){
		printf("Invalid input\n");
		return 1;
	}

	struct gps_location loc;

	loc.lat_integer = atoi(argv[1]);
	loc.lat_fractional = atoi(argv[2]);
	loc.lng_integer = atoi(argv[3]);
	loc.lng_fractional = atoi(argv[4]);
	loc.accuracy = atoi(argv[5]);

	if(syscall(SET_GPS_LOCATION, &loc)){
		printf("Invalid location, rejected\n");
		return 1;
	}

	printf("New location succesfully set\n");

	return 0;
}
