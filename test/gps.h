#ifndef _LINUX_GPS_H
#define _LINUX_GPS_H

struct gps_location {
	int lat_integer;
	int lat_fractional;
	int lng_integer;
	int lng_fractional;
	int accuracy;
};

#endif
