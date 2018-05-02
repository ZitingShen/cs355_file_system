#ifndef __FS_H__
#define __FS_H__

#include "util.h"
#include <sys/stat.h>
#include <sys/types.h>

struct disk_image {
	int id;
	struct superblock sb;
	struct inode *inodes;
	struct disk_image *next;
};

/* Return file descriptor on success. -1 on failure and set errno. */
int f_open(const char *path, const char *mode);

size_t f_read(void *ptr, size_t size, size_t nitems, int fd);

size_t f_write(const void *ptr, size_t size, size_t nitems, int fd);

int f_close(int fd);

int f_seek(int fd, long offset, int whence);

void f_rewind(int fd);

int f_stat(int fd, struct stat *buf);

int f_remove(const char *path);

/* Return file descriptor on success. -1 on failure and set errno. */
int f_opendir(const char *path);

struct dirent *f_readdir(int fd);

int f_closedir(int fd);

int f_mkdir(const char *path, mode_t mode);

int f_rmdir(const char *path);

/*
Available flags (used with data):
*/
int f_mount(const char *source, const char *target, int flags, void *data);

/*
Available flags:
*/
int f_umount(const char *target, int flags);

#endif