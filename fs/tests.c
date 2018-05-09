#include "fs.h"
#include "fs_debug.h"
#include <stdio.h>
#include <stdlib.h>
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
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);
	print_disks();
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_open_nested() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make /usr");
	fd = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(fd >= 0);

	printf("%s\n", "Create and open /usr/design.txt");
	fd = f_open("/usr/design.txt", "w");
	assert(fd >= 0);
	print_disks();
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_open_relative() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Open /usr");
	fd = f_opendir("/usr");
	assert(fd >= 0);
	pwd_fd = fd;

	printf("%s\n", "Create and open /usr/design.txt by its relative path \'design.txt\'");
	fd = f_open("design.txt", "w");
	assert(fd >= 0);
	print_disks();
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_open_existing() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Close /design.txt");
	result = f_close(fd);
	assert(fd == 0);
	print_fd(fd);

	printf("%s\n", "Open existing /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_close() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Close /design.txt");
	result = f_close(fd);
	assert(fd == 0);
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_read() {
	int result;
	int fd, fd_usr;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Open /usr");
	fd_usr = f_opendir("/usr");
	assert(fd_usr >= 0);

	printf("%s\n", "Open /");
	fd = f_opendir("/");
	assert(fd >= 0);
	
	printf("%s\n", "Read an int from /");
	int i;
	result = f_read(&i, sizeof(int), 1, fd);
	printf("%d\n", result);
	assert(result == sizeof(int));
	
	printf("i should equal to the inode index in the following file entry:\n");
	printf("i: %d\n", i);
	print_fd(fd_usr);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_read_more() {
	// TODO
}

void test_f_write() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);

	printf("%s\n", "Write an integer 4242 to /design.txt");
	int i = 4242;
	result = f_write(&i, sizeof(int), 1, fd);
	assert(result == sizeof(int));
	print_disks();
	print_fd(fd);

	printf("%s\n", "Rewind the offset of /design.txt");
	f_rewind(fd);

	printf("%s\n", "Read an integer from /design.txt and make sure it's 4242");
	int j;
	result = f_read(&j, sizeof(int), 1, fd);
	assert(result == sizeof(int));
	printf("%d\n", j);
	assert(i == j);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_write_multiple_times() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);

	printf("%s\n", "Write integer 1000 to 1999 to /design.txt, one at a time");
	int i;
	int size = 1000;
	for(int k = 0; k < size; k++) {
		i = k+1000;
		result = f_write(&i, sizeof(int), 1, fd);
		assert(result == sizeof(int));
	}
	print_disks();
	print_fd(fd);

	printf("%s\n", "Rewind the offset of /design.txt");
	f_rewind(fd);

	printf("%s\n", "Read an integer 1000 times from /design.txt and make sure it matches the original ones");
	int j;
	for(int k = 0; k < size; k++) {
		result = f_read(&j, sizeof(int), 1, fd);
		assert(result == sizeof(int));
		assert(j == (k+size));
	}
	printf("%s\n", "OK");

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_write_multiple_bytes() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);

	printf("%s\n", "Write an integer array of size 1000 to /design.txt.");
	printf("%s\n", "The numbers are contiguous integers 1000 to 1999");
	int size = 1000;
	int arr[size];
	for(int k = 0; k < size; k++) {
		arr[k] = k+1000;
	}
	result = f_write(&(arr[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	print_disks();
	print_fd(fd);

	printf("%s\n", "Rewind the offset of /design.txt");
	f_rewind(fd);

	printf("%s\n", "Read the integer array from /design.txt and make sure it's the original array");
	int arr2[1000];
	result = f_read(&(arr2[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	for(int k = 0; k < size; k++) {
		assert(arr2[k] == arr[k]);
	}
	printf("%s\n", "OK");

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_write_iblocks() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);

	printf("%s\n", "Write an integer array of size 2000 to /design.txt.");
	printf("%s\n", "The numbers are contiguous integers 1000 to 2999");
	int size = 2000;
	int arr[size];
	for(int k = 0; k < size; k++) {
		arr[k] = k+1000;
	}
	result = f_write(&(arr[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	print_disks();
	print_fd(fd);

	printf("%s\n", "Rewind the offset of /design.txt");
	f_rewind(fd);

	printf("%s\n", "Read the integer array from /design.txt and make sure it's the original array");
	int arr2[size];
	result = f_read(&(arr2[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*2000);
	for(int k = 0; k < size; k++) {
		assert(arr2[k] == arr[k]);
	}
	printf("%s\n", "OK");

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_write_i2block() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);

	printf("%s\n", "Write an integer array of size 2000000 to /design.txt.");
	printf("%s\n", "The numbers are contiguous integers 1000 to 2000999");
	int size = 2000000;
	int *arr = malloc(sizeof(int)*size);
	for(int k = 0; k < size; k++) {
		arr[k] = k+1000;
	}
	result = f_write(&(arr[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	//print_disks();
	print_fd(fd);

	printf("%s\n", "Rewind the offset of /design.txt");
	f_rewind(fd);

	printf("%s\n", "Read the integer array from /design.txt and make sure it's the original array");
	int *arr2 = malloc(sizeof(int)*size);
	result = f_read(&(arr2[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	for(int k = 0; k < size; k++) {
		assert(arr2[k] == arr[k]);
	}
	printf("%s\n", "OK");
	free(arr);
	free(arr2);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_write_i3block() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Create and open /design.txt");
	fd = f_open("/design.txt", "w");
	assert(fd >= 0);

	printf("%s\n", "Write an integer array of size 20000 to /design.txt.");
	printf("%s\n", "The numbers are contiguous integers 1000 to 20999");
	int size = 3000000;
	int *arr = malloc(sizeof(int)*size);
	for(int k = 0; k < size; k++) {
		arr[k] = k+1000;
	}
	result = f_write(&(arr[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	//print_disks();
	print_fd(fd);

	printf("%s\n", "Rewind the offset of /design.txt");
	f_rewind(fd);

	printf("%s\n", "Read the integer array from /design.txt and make sure it's the original array");
	int *arr2 = malloc(sizeof(int)*size);
	result = f_read(&(arr2[0]), sizeof(int), size, fd);
	assert(result == sizeof(int)*size);
	for(int k = 0; k < size; k++) {
		assert(arr2[k] == arr[k]);
	}
	printf("%s\n", "OK");
	free(arr);
	free(arr2);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_seek() {
	int result;
	int fd;
	struct file_entry subfile;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /bin");
	result = f_mkdir("/bin", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Open root directory");
	fd = f_opendir("/");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /usr");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /bin");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);

	printf("%s\n", "Seek the offset of / to 1");
	result = f_seek(fd, 1, SEEK_SET);
	assert(result == 1);
	print_fd(fd);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /bin");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_rewind() {
	int result;
	int fd;
	struct file_entry subfile;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /bin");
	result = f_mkdir("/bin", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Open root directory");
	fd = f_opendir("/");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /usr");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /bin");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);

	printf("%s\n", "Seek the offset of / to 1");
	f_rewind(fd);
	print_fd(fd);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /usr");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_stat() {
	int result;
	int fd;
	struct stat buf;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /usr/bin");
	result = f_mkdir("/usr/bin", PERMISSION_DEFAULT);
	assert(result == 0);
	
	printf("%s\n", "Open /usr/bin");
	fd = f_opendir("/usr/bin");
	assert(fd >= 0);
	
	printf("%s\n", "Stat /usr/bin");
	result = f_stat(fd, &buf);
	assert(result >= 0);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_remove() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make file /design.txt");
	fd = f_open("/design.txt", "w");
	assert(result >= 0);

	result = f_close(fd);
	assert(result == 0);

	printf("%s\n", "Remove file /design.txt");
	result = f_remove("/design.txt");
	assert(result == 0);
	print_disks();
}

void test_f_remove_crazy() {
	int result;
	int fd;
	char filename[12];
	int size = 100;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make file /txt<index> for 100 times");
	for(int i = 0; i < size; i++) {
		sprintf(filename, "/txt%d", i);
		fd = f_open(filename, "w");
		assert(fd >= 0);
		result = f_close(fd);
		assert(result == 0);
	}

	printf("%s\n", "Remove all the files created");
	for(int i = 0; i < size; i++) {
		sprintf(filename, "/txt%d", i);
		result = f_remove(filename);
		if(result != 0)
			printf("%d\n", i);
		assert(result == 0);
	}

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
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

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_opendir_absolute() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Open /usr");
	fd = f_opendir("/usr");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_opendir_absolute_nested() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /usr/bin");
	result = f_mkdir("/usr/bin", PERMISSION_DEFAULT);
	assert(result == 0);
	print_disks();

	printf("%s\n", "Open /usr");
	fd = f_opendir("/usr");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Open /usr/bin");
	fd = f_opendir("/usr/bin");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_opendir_relative() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /usr/bin");
	result = f_mkdir("/usr/bin", PERMISSION_DEFAULT);
	assert(result == 0);
	print_disks();

	printf("%s\n", "Open /usr");
	fd = f_opendir("/usr");
	assert(fd >= 0);
	print_fd(fd);
	pwd_fd = fd;

	printf("%s\n", "Open /usr/bin by relative path \'bin\'");
	fd = f_opendir("bin");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_readdir() {
	int result;
	int fd;
	struct file_entry subfile;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Open root directory");
	fd = f_opendir("/");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Read /");
	printf("%s\n", "Should successfully read /usr");
	subfile = f_readdir(fd);
	assert(subfile.node >= 0);
	print_file_entry(&subfile);
	print_disks();

	printf("%s\n", "Read / again");
	printf("%s\n", "Should fail to read");
	subfile = f_readdir(fd);
	assert(subfile.node < 0);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_readdir_crazy() {
	int result;
	int fd;
	struct file_entry subfile;
	char filename[12];
	int size = 100;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr<index> for 100 times");
	for(int i = 0; i < size; i++) {
		sprintf(filename, "/usr%d", i);
		result = f_mkdir(filename, PERMISSION_DEFAULT);
		assert(result == 0);
	}

	printf("%s\n", "Open root directory");
	fd = f_opendir("/");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Read / for 100 times");
	printf("%s\n", "Should successfully read /usr<index>");
	for(int i = 0; i < size; i++) {
		subfile = f_readdir(fd);
		assert(subfile.node >= 0);
	}

	printf("%s\n", "Read / again");
	printf("%s\n", "Should fail to read");
	subfile = f_readdir(fd);
	assert(subfile.node < 0);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_closedir() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Open root directory");
	fd = f_opendir("/");
	assert(fd >= 0);
	print_fd(fd);

	printf("%s\n", "Close root directory");
	result = f_closedir(fd);
	assert(result == 0);
	print_fd(fd);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
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

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
}

void test_f_rmdir() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make directory /usr");
	result = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /usr/bin");
	result = f_mkdir("/usr/bin", PERMISSION_DEFAULT);
	assert(result == 0);

	printf("%s\n", "Make directory /usr/design.txt");
	fd = f_open("/usr/bin", "w");
	assert(fd >= 0);
	result = f_close(fd);
	assert(result == 0);

	printf("%s\n", "Remove directory /usr");
	result = f_rmdir("/usr");
	assert(result == 0);

	printf("%s\n", "Unmount empty-disk");
	result = f_umount(0, 0);
	assert(result == 0);
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

	// printf("%s\n", "test close root directory");
	// test_f_closedir();
	// printf("\n");

	// printf("%s\n", "test read root directory");
	// test_f_readdir();
	// printf("\n");

	// printf("%s\n", "test read root directory for 100 times");
	// test_f_readdir_crazy();
	// printf("\n");

	// printf("%s\n", "test open /usr directory");
	// test_f_opendir_absolute();
	// printf("\n");

	// !!!! error in valgrind for mysterious reason
	// printf("%s\n", "test open /usr/bin directory");
	// test_f_opendir_absolute_nested();
	// printf("\n");

	// !!!! error in valgrind for mysterious reason
	// printf("%s\n", "test open /usr/bin directory by relative path");
	// test_f_opendir_relative();
	// printf("\n");

	// printf("%s\n", "test seek root directory");
	// test_f_seek();
	// printf("\n");

	// printf("%s\n", "test rewind root directory");
	// test_f_rewind();
	// printf("\n");

	// !!!! error in valgrind for mysterious reason
	// printf("%s\n", "test stat root directory");
	// test_f_stat();
	// printf("\n");

	// !!!! error in valgrind for mysterious reason
	// printf("%s\n", "test create and open /design.txt");
	// test_f_open();
	// printf("\n"); 

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test create and open /usr/design.txt");
	// test_f_open_nested();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test create and open /usr/design.txt by its relative path");
	// test_f_open_relative();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test close /design.txt after opening it");
	// test_f_close();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test open existing /design.txt after creating it");
	// test_f_open_existing();
	// printf("\n");

	// printf("%s\n", "create /usr, and read an int from /.");
	// printf("%s\n", "the int should be the inode index of /usr");
	// test_f_read();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test write an integer 4242 into /design.txt.");
	// test_f_write();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test write integer 1000 to 1999, one at a time into /design.txt.");
	// test_f_write_multiple_times();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test write an integer array of size 1000 into /design.txt.");
	// test_f_write_multiple_bytes();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test write an integer array of size 2000 into /design.txt.");
	// printf("%s\n", "test of read and write on iblocks.");
	// test_f_write_iblocks();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test write an integer array of size 2000000 into /design.txt.");
	// printf("%s\n", "test of read and write on iblocks.");
	// test_f_write_i2block();
	// printf("\n");

	// !!!! another error in valgrind for mysterious reason
	// printf("%s\n", "test write an integer array of size 3000000 into /design.txt.");
	// printf("%s\n", "test of read and write on iblocks.");
	// test_f_write_i3block();
	// printf("\n");

	// printf("%s\n", "test create and remove /design.txt");
	// test_f_remove();
	// printf("\n");

	// printf("%s\n", "test create 100 files and remove them");
	// test_f_remove_crazy();
	// printf("\n");

	printf("%s\n", "test create /usr and a bunch subfiles/subdirectories and remove /usr");
	test_f_rmdir();
	printf("\n");
}