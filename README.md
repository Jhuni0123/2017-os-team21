# Project 4: team 21
## Syscall number

| System Call | number |
| -------- | :--------: |
| set_gps_location | 380 |
| get_gps_location | 381 |


## Add gps location data fields to ext2 file system

In `fs/ext2/ext2.h`:
- add gps location data fields as `__le32` type in `struct ext2_inode`, which is inode for disk
- add `struct gps_location` in `struct ext2_inode_info`, which is inode for memory

In `fs/ext2/inode.c`:
- add conversions for added data fields to and from and to `struct ext2_inode` and `struct ext2_inode_info`
- `ext2_iget()` is for conversion from disk to memory inode
- `__ext2_write_inode()` is for conversion from memory to disk inode

## Add inode operations

In `fs/ext2/file.c`:
- add `set_gps_location` and `get_gps_location` to `ext2_file_inode_operations`

In `include/linux/fs.h`:
- add `set_gps_location` and `get_gps_location` poitner to `struct inode_operations`

In `fs/ext2/inode.c`:
- define `ext2_set_gps_location` to update file location with `device_loc` with `gps_lock` locked
- define `ext2_get_gps_location` to get file location stored in inode

## Set gps location when a file is created / modified

For create, in `fs/ext2/namei.c`, `ext2_create`
- check if it's ext2 file, and if is, call `set_gps_location` of `inode->i_op`

For modify, in `fs/read_write.c`, `vfs_write`
- check if it's ext2 file, and if is, call `set_gps_location` of `inode->i_op`

## `include/linux/gps.h`

- `struct gps_location` defined
- `device_loc` and `gps_lock` defined as extern values
- `is_same_location()` defined as extern function 

## `kernel/gps.c`
- `struct gps_location device_loc`: device's current location (shared source)
- `spinlock_t gps_lock`: spin lock for `device_loc`

define syscall operations:
- do_set_gps_location: update `device_loc` with `gps_lock` locked
- do_get_gps_location: return file's gps_location if user has read permission to it. get file path from pathname with `kern_path`, and check read permission with `inode_permission`

`bool is_same_location(loc1, loc2)`: determine whether two gps locations are in same location
- calculate the distance of two gps location
  - use the Pythagorean theorem
  - approximate the sine function(`int sin(theta)`) with 3rd taylor approximation series
  - short distances are almost precise
  - for longer distances(longer than about 10^6 meters), if one location is close to south/north pole, the calculated distance can be upto about 2 times longer than the real distance
```
x = (λ2-λ1) * sin(pi/2 - (φ1+φ2)/2);
y = (φ2-φ1);
d = sqrt(x*x + y*y) * R;
```
## Test codes

all test codes are in `/test/`

For spec fulfillment:
- build: `make`
- file_loc.c: get a pathname as command line argument, print gps corrdinates and google maps link
- gpsupdate.c: get lat_integer, lat_fractional, lng_integer, lng_fractional, accuracy as command line arguments and update `device_loc` with this info.

For our own unit test purpose:
- build: `make test`
- getfileloc.c, readfile.c, writefile.c, runtest.sh


## `proj4.fs` structure and `file_loc` results

proj4
|- bldg_301
|- bldg_302
|- statue_of_liberty
|- stations
___|- seoul_natl_univ
___|- nakseongdae
___|- mangu

owner:~/proj4> ../file_loc bldg_301
GPS coordinates: lat: 37.449883, lng: 126.052493, accuracy: 30 m
Google Maps link: www.google.com/maps/search/37.449883,126.952493

owner:~/proj4> ../file_loc bldg_302
GPS coordinates: lat: 37.448689, lng: 126.052536, accuracy: 30 m
Google Maps link: www.google.com/maps/search/37.448689,126.952536

owner:~/proj4> ../file_loc statue_of_liberty
GPS coordinates: lat: 40.689229, lng: -74.044559, accuracy: 30 m
Google Maps link: www.google.com/maps/search/40.689229,-74.044559

owner:~/proj4> ../file_loc stations
No GPS info found

owner:~/proj4> ../file_loc stations/seoul_natl_univ
GPS coordinates: lat: 37.481218, lng: 126.052734, accuracy: 30 m
Google Maps link: www.google.com/maps/search/37.481218,126.952734

owner:~/proj4> ../file_loc stations/nakseongdae
GPS coordinates: lat: 37.477022, lng: 126.063557, accuracy: 40 m
Google Maps link: www.google.com/maps/search/37.477022,126.963557

owner:~/proj4> ../file_loc stations/mangu
GPS coordinates: lat: 37.599268, lng: 127.092369, accuracy: 40 m
Google Maps link: www.google.com/maps/search/37.599268,127.092369

## Demo video links
TODO: add video link
