# RAMDISK - An in memory file system

This project deals with building an in-memory Linux file system using the FUSE (File System in User Space) library to interact with the kernel. The file system is designed to be persistent. When the file system is unmounted, the data is written to the secondary storage as a binary blob. This blob can be re-mounted to access the previous saved state of the file system.

The following command can be used to mount the file system:
./ramdisk <path-to-root-dir> <size-in-MB> <persistent-file>[optional]

