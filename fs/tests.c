#include "fs.h"
#include "fs_debug.h"
#include <stdio.h>
#include <assert.h> 
int pwd_fd;
int uid;

void test_mount_umount() {
	int result;

	printf("%s\n", "Mount empty-disk and print all disks");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);
	print_disks();

	printf("%s\n", "Unmount empty-disk and print all disks");
	result = f_umount(0, 0);
	assert(result == 0);
	print_disks();
}

void test_f_open() {

}

void test_f_close() {

}

void test_f_read() {

}

void test_f_write() {

}

void test_f_seek() {

}

void test_f_rewind() {

}

void test_f_stat() {

}

void test_f_remove() {

}

void test_f_opendir_root() {
	int result;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Open root directory");
	result = f_opendir("/");
	assert(result >= 0);
	print_fd(result);
}

void test_f_opendir_absolute() {
	
}

void test_f_opendir_relative() {
	
}

void test_f_readdir() {

}

void test_f_closedir() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Open root directory");
	fd = f_opendir("/");
	assert(result >= 0);
	print_fd(fd);

	printf("%s\n", "Close root directory");
	result = f_closedir(fd);
	assert(result == 0);
	print_fd(fd);
}

void test_f_mkdir() {
	int result;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);
	print_disks();
}

void test_f_rmdir() {

}

int main() {
	// printf("%s\n", "test f_mount and f_umount");
	// test_mount_umount();
	// printf("\n");

	// printf("%s\n", "test open root directory");
	// test_f_opendir_root();
	// printf("\n");

	// printf("%s\n", "test make directory under the root directory");
	// test_f_mkdir();
	// printf("\n");

	printf("%s\n", "test close root directory");
	test_f_closedir();
	printf("\n");
}