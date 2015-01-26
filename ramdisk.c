/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <errno.h>
#include <fcntl.h>
#include "ramdisk_util.h"

void fill_system_stat(struct stat *stbuf, Cluster* cluster, long cluster_number)
{
	struct fuse_context* fc = fuse_get_context();

	stbuf -> st_dev = 100;
	stbuf -> st_ino = cluster_number;
	stbuf -> st_nlink = 1;
	stbuf -> st_size = find_size(cluster);
	stbuf -> st_uid = fc -> uid;
	stbuf -> st_gid = fc -> gid;
	stbuf -> st_blksize = 512;
	stbuf -> st_blocks = 1;
	stbuf -> st_atime = stbuf -> st_ctime = stbuf -> st_mtime = time(NULL);

	//stbuf -> st_mode = fetched_inode -> mode;
	if(cluster->type == 'D')
		stbuf -> st_mode = S_IFDIR | 0755;
	else if(cluster->type == 'F')
		stbuf -> st_mode = S_IFREG | 0755;
}

void read_directory(const char* path, void *buf, fuse_fill_dir_t filler) {

	long parent_cluster_number = find_dir_file_cluster(path);
	Cluster* parent = find_cluster(parent_cluster_number);
	if(parent->child_cluster != 0)
		traverse_directory(parent->child_cluster, buf, filler);
}


void traverse_directory(long start_cluster_number, void *buf, fuse_fill_dir_t filler) {

	long current_file_dir = start_cluster_number;
	Cluster* cluster = find_cluster(current_file_dir);
	struct stat stbuf;

	while(cluster) {
		memset(&stbuf, 0, sizeof(struct stat));
		fill_system_stat(&stbuf, cluster, current_file_dir);
		if (filler(buf, cluster->name, &stbuf, 0))
			break;
		else
			printf("insertung node: %s\n", cluster->name);

		current_file_dir = cluster->sibling_cluster;
		if(current_file_dir == 0)
			break;
		cluster = find_cluster(current_file_dir);
	}
}

static int rmdk_getattr(const char *path, struct stat *stbuf)
{
	printf("inside rmdk_getattr: path: %s\n", path);
	int res = 0;
	//struct stat stbuf;

	memset(stbuf, 0, sizeof(struct stat));
	long cluster_number = find_dir_file_cluster(path);
	Cluster* cluster = find_cluster(cluster_number);
	if(cluster_number != MAX_CLUSTER_NUMBER) {

		fill_system_stat(stbuf, cluster, cluster_number);
	}
	else
		res = -ENOENT;

	printf("exit rmdk_getattr\n");
	return res;
}

static int rmdk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	read_directory(path, buf, filler);

	return 0;
}

static int rmdk_mkdir(const char *path, mode_t mode)
{
	if(mode){}
	long dir_cluster = create_directory(path);
	if (dir_cluster == MAX_CLUSTER_NUMBER)
		return -1;

	return 0;
}

static int rmdk_rmdir(const char *path)
{
	long dir_cluster = remove_directory(path);
	if (dir_cluster == (MAX_CLUSTER_NUMBER + 1))
		return -ENOTEMPTY;
	else if(dir_cluster == MAX_CLUSTER_NUMBER)
		return -1;

	return 0;
}

static int rmdk_mknod(const char *path, mode_t mode, dev_t rdev)
{
	if(mode && rdev) {};

	long file_cluster = create_file(path);
	if (file_cluster == (MAX_CLUSTER_NUMBER + 1))
		return -ENOMEM;
	else if(file_cluster == MAX_CLUSTER_NUMBER)
		return -1;

	return 0;
}

static int rmdk_open(const char *path, struct fuse_file_info *fi)
{
	(void) fi;

	if(path){}
	/*if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;
	*/
	return 0;
}


static int rmdk_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//printf("inside read: size: %u  offset:%u\n", size, offset);
	(void) fi;

	size = handle_read_data(path, buf, size, offset);
	//printf("Data read from file: %d\n",size);

	int count = 0;
	while (buf[count] != '\0') {
		count++;
	}
	//printf("actual data in buffer: %d\n", count);

	return size;
}

static int rmdk_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) fi;

	int ret_size = handle_write_data(path, buf, size, offset);
	if(ret_size == 0)
		return -EFBIG;
	printf("size of data written: %d\n", ret_size);

	return ret_size;
}

static int rmdk_truncate(const char *path, off_t size)
{
	printf("inside truncate: %d\n", size);
	/*int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;
	*/
	return 0;
}

static int rmdk_access(const char *path, int mask)
{
	return 0;
}

static int rmdk_unlink(const char *path)
{
	long ret = remove_file(path);
	if(ret == -1)
		return -ENOENT;

	return 0;
}

static void rmdk_destroy() {

	//printf("destory called\n");

	if(ramdisk_fs_inputs -> fs_load_file) {
		FILE* write_fs_file = fopen(ramdisk_fs_inputs -> fs_load_file, "wb");
		if(write_fs_file) {
			//printf("found file\n");
			fwrite(ramdisk_root->root_cluster, ((ramdisk_fs_inputs -> fs_size) * 1024  * 1024), 1, write_fs_file);
			fclose(write_fs_file);
		}
	}
}

void load_fs_from_file() {
	if(ramdisk_fs_inputs -> fs_load_file) {
		FILE* read_fs_from_file = fopen(ramdisk_fs_inputs -> fs_load_file, "r");
		if(read_fs_from_file) {
			size_t read_data = fread(ramdisk_root->root_cluster, ((ramdisk_fs_inputs -> fs_size) * 1024  * 1024), 1, read_fs_from_file);
			if(read_data){}
			fclose(read_fs_from_file);
		}
	}
}

static struct fuse_operations hello_oper = {
	.getattr	= rmdk_getattr,
	.readdir	= rmdk_readdir,
	.mkdir 		= rmdk_mkdir,
	.rmdir 		= rmdk_rmdir,
	.mknod		= rmdk_mknod,
	.open		= rmdk_open,
	.read		= rmdk_read,
	.write		= rmdk_write,
	.truncate	= rmdk_truncate,
//	.access		= rmdk_access,
	.unlink		= rmdk_unlink,
	.destroy	= rmdk_destroy,
};

int main(int argc, char *argv[])
{
	init_ramdisk(argc, argv);
	load_fs_from_file();

	if(argc == 3)
		argc -= 1;
	else if(argc == 4)
		argc -= 2;

	return fuse_main(argc, argv, &hello_oper, NULL);
}
