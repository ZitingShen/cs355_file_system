#ifndef __FS_H__
#define __FS_H__

#ifdef __cplusplus
    extern "C" {
#endif

#include "util.h"
#include <sys/stat.h>
#include <sys/types.h>

#define PATH_ROOT			'/'
#define	PATH_DELIM			"/"
#define OPEN_FILE_MAX		100000
#define FILE_INDEX_LENGTH	sizeof(int)
#define FILE_NAME_LENGTH	12
#define	FILE_ENTRY_SIZE		(FILE_INDEX_LENGTH+FILE_NAME_LENGTH)
#define DIR_INIT_OFFSET		2

struct disk_image {
	int id;
	int fd; /* fd for the disk image file*/
	struct superblock sb;
	struct inode *inodes;
	struct disk_image *next;
};

struct open_file {
	int node;
	int offset;
	/**************************************************
	Offset is the count of file entries for directory
	file, and is the count of bytes otherwise.
	**************************************************/
	int mode;
	/**************************************************
	  ┌─────────────┬───────────────────────────────┐
      │f_open() mode│ int mode                      │
      ├─────────────┼───────────────────────────────┤
      │     r       │ O_RDONLY                      │
      ├─────────────┼───────────────────────────────┤
      │     w       │ O_WRONLY | O_CREAT | O_TRUNC  │
      ├─────────────┼───────────────────────────────┤
      │     a       │ O_WRONLY | O_CREAT | O_APPEND │
      ├─────────────┼───────────────────────────────┤
      │     r+      │ O_RDWR                        │
      ├─────────────┼───────────────────────────────┤
      │     w+      │ O_RDWR | O_CREAT | O_TRUNC    │
      ├─────────────┼───────────────────────────────┤
      │     a+      │ O_RDWR | O_CREAT | O_APPEND   │
      └─────────────┴───────────────────────────────┘
	**************************************************/
};

struct file_entry {
	int node;
	char file_name[FILE_NAME_LENGTH];
};

struct data_block {
	void *data;
	int data_addr;
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

struct file_entry f_readdir(int fd);

int f_closedir(int fd);

int f_mkdir(const char *path, int permission);

int f_rmdir(const char *path);

/*
Available flags (used with data):
*/
int f_mount(const char *source, const char *target, int flags, void *data);

/*
Available flags:
*/
int f_umount(const char *target, int flags);

/*------------helper functions for shell-------------------------*/
char* get_path(int inode_idx);

int change_file_mode(const char *path, int permissions);

int convert_mode(const char* mode);

char get_file_type(int inode_addr);

int get_file_permission(int inode_addr);

int get_file_uid(int inode_addr);

int get_file_size(int inode_addr);

#ifdef __cplusplus
}
#endif

#endif