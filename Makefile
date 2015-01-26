all:
	gcc -Wall ramdisk.c `pkg-config fuse --cflags --libs` -o ramdisk
