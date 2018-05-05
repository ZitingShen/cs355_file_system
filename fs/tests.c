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
	assert(result != -1);
	print_disks();

	printf("%s\n", "Unmount empty-disk and print all disks");
	result = f_umount(0, 0);
	assert(result != -1);
	print_disks();
}

int main() {
	test_mount_umount();
}