#include <linux/gps.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/namei.h>
#include <linux/math64.h>

struct gps_location device_loc;

DEFINE_SPINLOCK(gps_lock);

/* 
 * input value is (0~1000000) : (0~pi/2)
 * return value is 10^8 * sin
 */

int gps_sin(int theta)
{
	long long tmp1 = 2400000000000LL - (long long)theta*theta;
	tmp1 = tmp1 * theta;
	int tmp2 = 15278874;
	do_div(tmp1, tmp2);
	tmp2 = 1000;
	do_div(tmp1, tmp2);
	return (int)tmp1;
}

bool is_same_location(struct gps_location *loc1, struct gps_location *loc2)
{
	int lat1 = loc1->lat_integer*1000000 + loc1->lat_fractional;
	int lng1 = loc1->lng_integer*1000000 + loc1->lng_fractional;
	int lat2 = loc2->lat_integer*1000000 + loc2->lat_fractional;
	int lng2 = loc2->lng_integer*1000000 + loc2->lng_fractional;

	if (lat1 == lat2 && lng1 == lng2)
		return true;

	int lat_d = max(lat1, lat2) - min(lat1, lat2);
	int lng_d = max(lng1, lng2) - min(lng1, lng2);
	lng_d = min(lng_d, 360000000 - lng_d);
	
	int lat_avg;
	if (lat1 * lat2 < 0) {
		lat_avg = (abs(lat1) + abs(lat2)) / 3;
	}
	else {
		lat_avg = abs((lat1 + lat2) / 2);
	}

	int sin = gps_sin((90000000 - lat_avg) / 90);

	long long tmp1 = (long long)lng_d * sin;
	int tmp2 = 100000000;
	do_div(tmp1, tmp2);
	int lng_p = (int)tmp1;

	long long lat_l = (long long)lat_d;
	long long lng_l = (long long)lng_p;

	long long dist_sq = lat_l*lat_l + lng_l*lng_l;
	long long acc_sum = (loc1->accuracy + loc2->accuracy) * 899322LL;
	int divisor = 100000;
	do_div(acc_sum, divisor);

	long long acc_sq = (long long)acc_sum*acc_sum;

	return dist_sq <= acc_sq;
}



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
	struct gps_location kloc;
	struct path path;
	struct inode *inode;
	char *name;
	long len;

	if(loc == NULL)
		return -EINVAL;

	// get file's gps location
	// check do_sys_open() in fs/open.c for reference

	if((len = strnlen_user(pathname, 1000000L)) == 0)
		return -EINVAL;

	name = (char *) kmalloc(sizeof(char) * len, GFP_ATOMIC);
	
	if (name == NULL)
		return -ENOMEM;

	if(strncpy_from_user(name, pathname, len) < 0){
		kfree(name);
		return -EFAULT;
	}
	if(kern_path(name, 0, &path)){
		kfree(name);
		return -EINVAL;
	}

	kfree(name);
	inode = path.dentry->d_inode;
	
	if(inode->i_op->get_gps_location)
		inode->i_op->get_gps_location(inode, &kloc);
	else 
		return -ENODEV;
	
	if(copy_to_user(loc, &kloc, sizeof(struct gps_location)))
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

