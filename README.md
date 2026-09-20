# KryfsMountSystem
FUSE-based host-side mount and management utility for KryonOS's custom KRYFS file system, enabling direct read/write access.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Format and mount

Format creates or resets the image. The default image has 2048 blocks of 512 bytes.

```sh
./build/kfs /path/to/disk-kryfs.img --format
mkdir -p /tmp/kryfs
./build/kfs -m /path/to/disk-kryfs.img /tmp/kryfs
```

The mounted directory supports normal Linux file operations:

```sh
echo "hello" > /tmp/kryfs/hello.txt
cp apps/testapp/build/testapp.kef /tmp/kryfs/testapp.kef
cat /tmp/kryfs/hello.txt
rm /tmp/kryfs/hello.txt
fusermount3 -u /tmp/kryfs
```

The host tool and kernel use the same packed KRYFS layout: 512-byte blocks,
superblock at block 0, inode table from block 1, data from block 256, and
256-byte inode filenames. Files receive contiguous data extents and are moved
when they grow beyond their current extent.
