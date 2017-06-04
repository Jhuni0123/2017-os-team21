#include <linux/gps.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

struct gps_location device_loc;

DEFINE_SPINLOCK(gps_lock);

int do_set_gps_location(struct gps_location __user *loc)
{
	struct gps_location kloc;
	if(copy_from_user(&kloc, loc, sizeof(struct gps_location)))
		return -EFAULT;
	if(kloc.lat_integer < -90 || kloc.lat_integer > 90)
		return -EINVAL;
	if(kloc.lng_integer < -180 || kloc.lng_integer > 180)
		return -EINVAL;
	if(kloc.lat_fractional < 0 || kloc.lat_fractional > 999999)
		return -EINVAL;
	if(kloc.lng_fractional < 0 || kloc.lng_fractional > 999999)
		return -EINVAL;
	if(abs(kloc.lat_integer) == 90 && kloc.lat_fractional != 0)
		return -EINVAL;
	if(abs(kloc.lng_integer) == 180 && kloc.lng_fractional != 0)
		return -EINVAL;
	if(kloc.accuracy < 0)
		return -EINVAL;
	spin_lock(&gps_lock);
	memcpy(&device_loc, &kloc, sizeof(struct gps_location));
	spin_unlock(&gps_lock);
	return 0;
}

int do_get_gps_location(const char __user *pathname, struct gps_location __user *loc)
{
	if(loc == NULL)
		return -EINVAL;

	// get file's gps location
	// check do_sys_open() in fs/open.c for reference

	struct gps_location* kloc;
	if(copy_to_user(loc, kloc, sizeof(struct gps_location)))
		return -EFAULT;
	return 0;
}

SYSCALL_DEFINE1(set_gps_location, struct gps_location __user *, loc)
{
	return do_set_gps_location(loc);
}

SYSCALL_DEFINE2(get_gps_location, const char __user *, pathname, struct gps_location __user *, loc)
{
	return do_get_gps_location(pathname, loc);
}

