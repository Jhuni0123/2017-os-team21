#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>
#include "gps.h"

#define GET_GPS_LOCATION 381

int main(int argc, char** argv)
{
	if(argc < 2){
		printf("Invalid input\n");
		return 1;
	}
	
	struct gps_location *loc;
	loc = malloc(sizeof(struct gps_location));
	
	if(syscall(GET_GPS_LOCATION, argv[1], loc)){
		if (errno == ENODEV)
			printf("No GPS info found\n");
		else if (errno == EACCES)
			printf("Cannot read File\n");
		else if (errno == EINVAL)
			printf("Invalid input\n");
		else if (errno == ENOMEM)
			printf("No memory available\n");
		else if (errno == EFAULT)
			printf("Segmentation fault\n");
		return 1;
	}

	int lat = loc->lat_integer * 1000000 + loc->lat_fractional;
	int lng = loc->lng_integer * 1000000 + loc->lng_fractional;

	printf("GPS coordinates: lat: %d.%06d, lng: %d.%06d, accuracy: %d m\n", lat/1000000, abs(lat)%1000000, lng/1000000, abs(lng)%100000, loc->accuracy);

	printf("Google Maps link: www.google.com/maps/search/%d.%06d,%d.%06d\n", lat/1000000, abs(lat)%1000000, lng/1000000, abs(lng)%1000000);
	
	free(loc);
	return 0;
}
