# Project 4: team 21
## Syscall number

| System Call | number |
| -------- | :--------: |
| set_gps_location | 380 |
| get_gps_location | 381 |


# Add gps location data fields to ext2 file system

In `fs/ext2/ext2.h`:
- add gps location data fields as `__le32` type in `struct ext2_inode`, which is inode for disk
- add `struct gps_location` in `struct ext2_inode_info`, which is inode for memory

In `fs/ext2/inode.c`:
- add conversions for added data fields to and from and to `struct ext2_inode` and `struct ext2_inode_info`
- `ext2_iget()` is for conversion from disk to memory inode
- `__ext2_write_inode()` is for conversion from memory to disk inode

# Add inode operations

In `fs/ext2/file.c`:
- add `set_gps_location` and `get_gps_location` to `ext2_file_inode_operations`

In `include/linux/fs.h`:
- add `set_gps_location` and `get_gps_location` poitner to `struct inode_operations`

In `fs/ext2/inode.c`:
- define `ext2_set_gps_location` to update file location with `device_loc` with `gps_lock` locked
- define `ext2_get_gps_location` to get file location stored in inode

# set gps location when a file is created / modified

For create, in `fs/ext2/namei.c`, `ext2_create`
- check if it's ext2 file, and if is, call `set_gps_location` of `inode->i_op`

For modify, in `fs/read_write.c`, `vfs_write`
- check if it's ext2 file, and if is, call `set_gps_location` of `inode->i_op`

# `include/linux/gps.h`

- `struct gps_location` defined
- `device_loc` and `gps_lock` defined as extern values

# `kernel/gps.c`
`struct gps_location device_loc`: device's current location (shared source)

`spinlock_t gps_lock`: spin lock for `device_loc`

define syscall operations:
- do_set_gps_location: update `device_loc` with `gps_lock` locked
- do_get_gps_location: return file's gps_location if user has read permission to it. get file path from pathname with `kern_path`, and check read permission with `inode_permission`

`bool is_same_location(loc1, loc2)`: determine whether two gps locations are in same locaion
- calculate the distance of two gps location
  - use the Pythagorean theorem
  - approximate the sine function(`int sin(theta)`) with 3rd taylor approximation series
  - short distances are almost precise 
  - from mideum distances, if one location is close to south/north pole, the error increases upto about 2 times
  
# test codes

all test codes are in `/test/`

For spec fulfillment:
- build: `make`
- file_loc.c: get a pathname as command line argument, print gps corrdinates and google maps link
- gpsupdate.c: get lat_integer, lat_fractional, lng_integer, lng_fractional, accuracy as command line arguments and update `device_loc` with this info.

For our own unit test purpose:
- build: `make test`
- getfileloc.c, readfile.c, writefile.c, runtest.sh


TODO: add video link, and others
