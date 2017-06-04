#include <linux/gps.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/namei.h>
#include <linux/fs.h>

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

	/* access permission check: read permission */
	if(inode_permission(inode, MAY_READ))
		return -EACCES;

	/* if passed, get gps location */
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

