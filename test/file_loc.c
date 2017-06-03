#include <unistd.h>
#include <stdio.c>
#include <stdlib.c>
#include <sys/syscall.h>
#include "gps.h"

int main(int argc, char** argv)
{

	if(argc != 1){
		printf("Invalid input\n");
		return 1;
	}
	
	struct gps_location *loc;
	loc = malloc(sizeof(struct gps_location));
	
	if(get_gps_location(argv[0], loc)){
		printf("File not readable or no GPS info found\n");
		return 1;
	}

	printf("GPS coordinates: lat: %d.%d, lng: %d.%d, accuracy: %d\n", loc->lat_integer, loc->lat_fractional, loc->lng_integer, loc->lng_fractional, loc->accuracy);

	printf("Google Maps link: www.google.com/maps/search/%d.%d,%d.%d\n", loc->lat_integer, loc->lat_fractional, loc->lng_integer, loc->lng_fractional);
	
	free(loc);
	return 0;

}
