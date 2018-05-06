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
}

void test_f_open_relative() {
	int result;
	int fd;

	printf("%s\n", "Mount empty-disk");
	result = f_mount("empty-disk", 0, 0, 0);
	assert(result == 0);

	printf("%s\n", "Make /usr");
	fd = f_mkdir("/usr", PERMISSION_DEFAULT);
	assert(fd >= 0);
	pwd_fd = fd;

	printf("%s\n", "Create and open /usr/design.txt by its relative path \'design.txt\'");
	fd = f_open("design.txt", "w");
	assert(fd >= 0);
	print_disks();
	print_fd(fd);
}

void test_f_open_after_close() {
	// TODO
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
}

void test_f_read() {
	// TODO
}

void test_f_write() {
	// TODO
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
}

void test_f_remove() {
	// TODO
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
	// TODO
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

	// printf("%s\n", "test open /usr directory");
	// test_f_opendir_absolute();
	// printf("\n");

	// printf("%s\n", "test open /usr/bin directory");
	// test_f_opendir_absolute_nested();
	// printf("\n");

	// printf("%s\n", "test open /usr/bin directory by relative path");
	// test_f_opendir_relative();
	// printf("\n");

	// printf("%s\n", "test seek root directory");
	// test_f_seek();
	// printf("\n");

	// printf("%s\n", "test rewind root directory");
	// test_f_rewind();
	// printf("\n");

	// printf("%s\n", "test rewind root directory");
	// test_f_stat();
	// printf("\n");

	// printf("%s\n", "test create and open /design.txt");
	// test_f_open();
	// printf("\n");

	// printf("%s\n", "test create and open /usr/design.txt");
	// test_f_open_nested();
	// printf("\n");

	printf("%s\n", "test create and open /usr/design.txt by its relative path");
	test_f_open_relative();
	printf("\n");
}