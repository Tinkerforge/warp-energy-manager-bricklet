#!/bin/bash
cd /opt/littlefs-fuse/
./lfs --block_size=512 --block_count=15333376 --block_cycles=-1 --read_size=512 --prog_size=512 --cache_size=512 --lookahead_size=512 /dev/sdc /home/olaf/tf/warp-energy-manager-bricklet/lfs_mount_dir
cd /home/olaf/tf/warp-energy-manager-bricklet/lfs_mount_dir
ls
