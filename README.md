# LnrfsMountSystem
FUSE-based host-side mount and management utility for KryonOS's custom LNRFS file system, enabling direct read/write access.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Format and mount

Format creates or resets the image. The default image has 2048 blocks of 512 bytes.

```sh
./build/lfs /path/to/disk-lnrfs.img --format
mkdir -p /tmp/lnrfs
./build/lfs -m /path/to/disk-lnrfs.img /tmp/lnrfs
```

The mounted directory supports normal Linux file operations:

```sh
echo "hello" > /tmp/lnrfs/hello.txt
cp apps/testapp/build/testapp.kef /tmp/lnrfs/testapp.kef
cat /tmp/lnrfs/hello.txt
rm /tmp/lnrfs/hello.txt
fusermount3 -u /tmp/lnrfs
```

The host tool and kernel use the same packed LNRFS layout: 512-byte blocks,
superblock at block 0, inode table from block 1, data from block 256, and
256-byte inode filenames. Files receive contiguous data extents and are moved
when they grow beyond their current extent.
