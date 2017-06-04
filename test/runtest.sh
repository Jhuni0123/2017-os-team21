#! /bin/bash

# file_loc should be tested individually
ext2="./"

rm "$ext2"f1
rm "$ext2"f2
rm "$ext2"f3
rm "$ext2"f4

##############################################

echo "#1: gpsupdate test"
echo 1-1
./gpsupdate
rv=$?; if [[ $rv == 0 ]]; then exit; fi

echo 1-2
./gpsupdate 90 0 -180 0 1
rv=$?; if [[ $rv != 0 ]]; then exit; fi

echo 1-3
./gpsupdate -90 0 179 999999 0
rv=$?; if [[ $rv != 0 ]]; then exit; fi

echo 1-4
./gpsupdate 90 1 180 0 0
rv=$?; if [[ $rv == 0 ]]; then exit; fi

echo 1-5
./gpsupdate -90 0 -180 1 0
rv=$?; if [[ $rv == 0 ]]; then exit; fi

echo 1-6
./gpsupdate 0 0 0 0 -1
rv=$?; if [[ $rv == 0 ]]; then exit; fi

##############################################

# make nonzero return value -> abort shell
set -e

echo "#2: create file test"
echo 2-1
./gpsupdate 0 0 0 0 5
touch "$ext2"f1
rv=$(./getfileloc "$ext2"f1)
if [ "$rv" != "0.0 0.0 5" ]; then exit; fi

echo 2-2
./gpsupdate 0 1 10 0 1
rv=$(./getfileloc "$ext2"f1)
if [ "$rv" != "0.0 0.0 5" ]; then exit; fi

# permission denied
echo 2-3
set +e
cat "$ext2"f1
rv=$?; if [[ $rv == 0 ]]; then exit; fi
set -e

echo 2-4
./gpsupdate 90 0 -180 0 3
touch "$ext2"f2
rv=$(./getfileloc "$ext2"f2)
if [ "$rv" != "90.0 -180.0 3" ]; then exit; fi

###############################################

echo "#3: modify file test"
echo 3-1
./gpsupdate 0 0 0 0 1
./writefile "$ext2"f1 modified
rv=$(./getfileloc "$ext2"f1)
if [ "$rv" != "0.0 0.0 1" ]; then exit; fi
rv=$(cat "$ext2"f1)
if [ "$rv" != "modified" ]; then exit; fi

# try write file in different location
# when file open was successful
echo 3-2
set +e
./writefile "$ext2"f1 this_shall_not_write 2 &
./sleep 1
./gpsupdate 45 999999 45 999999 10
./sleep 2
./gpsupdate 0 0 0 0 1
rv=$(cat "$ext2"f1)
if [ "$rv" != "modified" ]; then exit; fi

# same as 3-2 except this case is reading
echo 3-3
./gpsupdate 0 0 0 0 1
./readfile "$ext2"f1 2 &
./sleep 1
./gpsupdate 45 999999 45 999999 10
./sleep 2
./gpsupdate 0 0 0 0 1
rv=$(cat out)
if [ "$rv" != "12" ]; then exit; fi
set -e

###############################################

echo "#4: angle wrapping test"
./gpsupdate -90 0 180 0 1
touch "$ext2"f3
./gpsupdate 90 0 -180 0 1
./writefile "$ext2"f3 modified
./gpsupdate 90 0 -180 0 1
cat "$ext2"f3
# cat should not change file's gps_location
./gpsupdate -90 0 180 0 3
cat "$ext2"f3
rv=$(./getfileloc "$ext2"f3)
if [ "$rv" != "90 0 -180 0 1" ]; then exit; fi

###############################################

# assume (1 deg difference) == 110,000 meters
# assume range differences are calculated by surface distance

echo "#5: gps movement & accuracy range test"
echo 5-1
./gpsupdate 90 0 180 0 10000
./writefile "$ext2"f3 m1
./gpsupdate -89 999999 -179 999999 10000
./writefile "$ext2"f3 m2
./gpsupdate 89 89 179 179 200000
./writefile "$ext2"f3 m3

echo 5-2
# this range should fail
./gpsupdate 86 0 179 0 1
set +e
./writefile "$ext2"f3 m3
rv=$?; if [[ $rv == 0 ]]; then exit; fi
set -e
./gpsupdate -88 0 179 0 200000
./writefile "$ext2"f3 m4

echo 5-3
# this range should fail
./gpsupdate 0 0 0 0 39500000
set +e
./writefile "$ext2"f3 m4
rv=$?; if [[ $rv == 0 ]]; then exit; fi
set -e
./gpsupdate 0 0 0 0 40100000
./writefile "$ext2"f3 m5

###############################################

echo "#6: get_gps_location access check"
echo 6-1
set +e
./gpsupdate 0 0 0 0 1
touch "$ext2"f4
chmod -r "$ext2"f4
./getfileloc "$ext2"f4
rv=$?; if [[ $rv == 0 ]]; then exit; fi

echo 6-2
chmod +r "$ext2"f4
./gpsupdate 1 0 -1 0 1
./getfileloc "$ext2"f4
rv=$?; if [[ $rv == 0 ]]; then exit; fi
set -e

echo 6-3
./gpsupdate 0 0 0 0 1
chmod -w "$ext2"f4
./getfileloc "$ext2"f4

###############################################

echo "#7: even sudo user can't access"
set +e
./gpsupdate 90 0 180 0 1
sudo cat "ext2"f1
rv=$?; if [[ $rv == 0 ]]; then exit; fi
set -e

###############################################

echo "All tests passed!"

# there's an article that 'set -e' option is not reliable.
# so test it
./gpsupdate 90 0 180 0 1
./writefile "$ext2"f1 aaaaaa
cat "ext2"f1
echo "This statement should not be printed."

