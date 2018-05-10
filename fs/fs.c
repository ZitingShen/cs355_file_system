#include "fs.h"
#include "fs_debug.h"
#include "util.h"
#include "format.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

extern int pwd_fd;
extern int uid;
int root = 0;
struct disk_image *cur_disk = 0;
struct disk_image *disks = 0;
struct open_file open_files[OPEN_FILE_MAX];
int next_fd = 0;

size_t f_write_helper(const void *ptr, size_t size, size_t nitems, int fd, int offset_unit);
int increment_next_fd();
int convert_mode(const char* mode);
struct file_entry find_subfile(int dir_fd, char *file_name);
int create_file(int dir_fd, char *filename, int permission, char type);
int remove_file(int dir_fd, int file_removed_inode_idx);
int find_and_remove_entry(int dir_fd, int file_inode_idx);
int remove_directory(int dir_fd);

struct data_block load_block(int node_addr, int block_num);
struct data_block load_data_block(int block_addr);
struct data_block load_indirect_block(int iblock_addr, int block_num);
struct data_block load_i2block(int i2block_addr, int *block_num);
struct data_block load_i3block(int i3block_addr, int *block_num);

int write_block(int node_addr, int block_num, void * data);
void write_data_block(int block_addr, void * data);
void write_indirect_block(int iblock_addr, int block_num, void * data, int add_free[4]);
void write_i2block(int i2block_addr, int *block_num, void * data, int add_free[4]);
void write_i3block(int i3block_addr, int *block_num, void * data, int add_free[4]);
int* which_to_find_free (size_t file_block, int block_num);

void write_superblock();
void write_inode(int node);
void write_data(struct data_block *db);

void clean_all_block(struct inode *file_inode);
void clean_dblock(int data_block_addr, size_t *rem_size);
void clean_iblock(int iblock_addr, size_t *rem_size);
void clean_i2block(int i2block_addr, size_t *rem_size);
void clean_i3block(int i3block_addr, size_t *rem_size);

void add_free_block(int block_num);
int find_free_block();

int strend(const char *s, const char *t);

//***************************LIBRARY FUNCTIONS******************
int f_open(const char *path, const char *mode) {
	int mode_binary = convert_mode(mode);
	if (mode_binary < 0)
		return -1;
	char *path_copy = malloc(strlen(path)+1);
	strcpy(path_copy, path);
	char *seg = strtok(path_copy, PATH_DELIM);
	if((*path) == PATH_ROOT) { // absolute path
		open_files[next_fd].node = root;
		open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		open_files[next_fd].mode = O_RDONLY;
	} else { // relative path
		open_files[next_fd].node = open_files[pwd_fd].node;
		if(open_files[next_fd].node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
	}

	struct file_entry subfile;
	while(seg) {
		subfile = find_subfile(next_fd, seg);
		if(subfile.node < 0) {
			if(strend(path, seg) && (mode_binary & O_CREAT)) {
				subfile.node = create_file(next_fd, seg, PERMISSION_DEFAULT, TYPE_NORMAL);
				if(subfile.node < 0) {
					free(path_copy);
					return -1;
				}
			} else {
				free(path_copy);
				errno = ENOENT;
				return -1;
			}
		}

		open_files[next_fd].node = subfile.node;
		if(subfile.node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
		seg = strtok(NULL, PATH_DELIM);
	}

	int return_fd = next_fd;
	open_files[next_fd].offset = 0;
	open_files[return_fd].mode = mode_binary;
	open_files[return_fd].offset = cur_disk->inodes[open_files[return_fd].node].size;
	if(increment_next_fd() == -1) {
		free(path_copy);
		return -1;
	}
	free(path_copy);
	return return_fd;
}

size_t f_read(void *ptr, size_t size, size_t nitems, int fd){
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
		return -1;
	}
	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{
		int BLOCK_SIZE = (cur_disk->sb).size;
		//int N_POINTER = BLOCK_SIZE / sizeof(int);
		size_t rem_size = size * nitems; //total read size
		int file_offset = open_files[fd].offset; 
		size_t cur_out_offset = 0;

		struct data_block temp_data_block;
		int first_block = 0;
		int read_size = 0;

		/*if there is offset, need to deal with the first data block to be read*/
		if (file_offset != 0){
			size_t first_block_rem = file_offset % BLOCK_SIZE;
			first_block = file_offset / BLOCK_SIZE;
			if (first_block_rem != 0){
				size_t copy_size = BLOCK_SIZE - first_block_rem;
				if (rem_size < copy_size) copy_size = rem_size;

				temp_data_block = load_block(open_files[fd].node, first_block);
				memcpy(ptr + cur_out_offset, temp_data_block.data + first_block_rem, copy_size);
				free(temp_data_block.data);

				cur_out_offset += copy_size;
				rem_size -= copy_size;
				read_size += copy_size;
				first_block ++;
			}
		}

		int num_block = rem_size / BLOCK_SIZE;
		if (rem_size % BLOCK_SIZE != 0){
			num_block ++;
		}

		size_t remainder;
		/*read from data blocks*/
		for (int i = first_block; i < first_block + num_block; i++){
			if (rem_size <= 0) break;
			temp_data_block = load_block(open_files[fd].node, i);
			if(temp_data_block.data == 0) break;

			remainder = rem_size % BLOCK_SIZE;
			if (remainder == 0){
				remainder += BLOCK_SIZE;
			}

			memcpy(ptr + cur_out_offset, temp_data_block.data, remainder);
			cur_out_offset += remainder;
			rem_size -= remainder;
			read_size += remainder;
			free(temp_data_block.data);
		}
		open_files[fd].offset += nitems * size;
		return read_size;
	}
}

size_t f_write(const void *ptr, size_t size, size_t nitems, int fd){
	if (fd > OPEN_FILE_MAX){ //fd overflow
		errno = EBADF;
		return -1;
	}

	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	} else{
		struct inode * cur_inode = &((cur_disk->inodes)[open_files[fd].node]);
		int file_offset = open_files[fd].offset; 

		size_t write_size = f_write_helper(ptr, size, nitems, fd, open_files[fd].offset);
		// change offset
		open_files[fd].offset += write_size;

		//change size if we are writing outside of original data block range
		if (file_offset + write_size > cur_inode->size){
			cur_inode -> size = file_offset + write_size;
			write_inode(open_files[fd].node);
		}
		
		return write_size;
	}
}

/*can also close directory file with this function call*/
int f_close(int fd){
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
		return EOF;
	}
	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
		return EOF;
	}
	else{
		open_files[fd].node = -1;
		open_files[fd].offset = 0;
		open_files[fd].mode = -1;
		return 0;
	}
}

//success returns zero
int f_seek(int fd, long offset, int whence){
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
		return -1;
	}
	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{ //if valid inode stored in open_files
		long new_offset;
		if (whence == SEEK_SET){
			new_offset = offset;
		}
		else if(whence == SEEK_CUR){
			new_offset = open_files[fd].offset+ offset;
		}
		else{
			new_offset = ((cur_disk->inodes)[open_files[fd].node]).size + offset;
		}
		open_files[fd].offset = new_offset;
		return new_offset;
	}
}

void f_rewind(int fd){
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
	}

	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
	}
	else{
		if(open_files[fd].node == 0)
			open_files[fd].offset = DIR_INIT_OFFSET-1;
		else if(cur_disk->inodes[open_files[fd].node].type == TYPE_NORMAL)
			open_files[fd].offset = 0;
		else if(cur_disk->inodes[open_files[fd].node].type == TYPE_DIRECTORY)
			open_files[fd].offset = DIR_INIT_OFFSET;
	}
}

/*
sample output:
Information for testfile.sh
---------------------------
File Size:              36 bytes
Number of Links:        1
File inode:             180055
File Permissions:       -rwxr-xr-x
*/
int f_stat(int fd, struct stat *buf){
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
		return -1;
	}

	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{
		buf->st_dev = (uid_t) cur_disk->id;
		buf->st_ino = (ino_t) open_files[fd].node;
		buf->st_mode = (mode_t) cur_disk->inodes[open_files[fd].node].permission;
		if(cur_disk->inodes[open_files[fd].node].type == TYPE_DIRECTORY) {
			buf->st_mode += 10;
		}
		buf->st_nlink = (nlink_t) cur_disk->inodes[open_files[fd].node].nlink;
		buf->st_uid = (uid_t) cur_disk->inodes[open_files[fd].node].uid;
		buf->st_gid = (gid_t) cur_disk->inodes[open_files[fd].node].gid;
		buf->st_size = (off_t) cur_disk->inodes[open_files[fd].node].size;
		buf->st_blksize = (blksize_t) cur_disk->sb.size;
		buf->st_blocks = (blkcnt_t) buf->st_size/buf->st_blksize;
		if(buf->st_size % buf->st_blksize != 0) {
			buf->st_blocks++;
		}
		return 0;
	}
}

int f_remove(const char *path) {
	char *path_copy = malloc(strlen(path)+1);
	strcpy(path_copy, path);
	char *seg = strtok(path_copy, PATH_DELIM);
	if((*path) == PATH_ROOT) { // absolute path
		open_files[next_fd].node = root;
		open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		open_files[next_fd].mode = O_RDONLY;
	} else { // relative path
		open_files[next_fd].node = open_files[pwd_fd].node;
		if(open_files[next_fd].node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
	}

	struct file_entry subfile;
	while(seg) {
		subfile = find_subfile(next_fd, seg);
		if(subfile.node < 0) {
			free(path_copy);
			errno = ENOENT;
			return -1;
		}
		if(strend(path, seg)) {
			if(cur_disk->inodes[subfile.node].type == TYPE_DIRECTORY) {
				free(path_copy);
				return -1;
			}

			if(remove_file(next_fd, subfile.node) != 0) {
				free(path_copy);
				return -1;
			}
		}

		open_files[next_fd].node = subfile.node;
		if(subfile.node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
		seg = strtok(NULL, PATH_DELIM);
	}
	free(path_copy);
	return 0;
}

int f_opendir(const char *path) {
	char *path_copy = malloc(strlen(path)+1);
	strcpy(path_copy, path);
	char *seg = strtok(path_copy, PATH_DELIM);
	if((*path) == PATH_ROOT) { // absolute path
		open_files[next_fd].node = root;
		open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		open_files[next_fd].mode = O_RDONLY;
	} else { // relative path
		open_files[next_fd].node = open_files[pwd_fd].node;
		if(open_files[next_fd].node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
	}

	struct file_entry subfile;
	while(seg) {
		subfile = find_subfile(next_fd, seg);
		if(strend(path, seg)) {
			if(subfile.node < 0 || cur_disk->inodes[subfile.node].type != TYPE_DIRECTORY) {
				free(path_copy);
				errno = ENOENT;
				return -1;
			}
		} else {
			if(subfile.node < 0) {
				free(path_copy);
				errno = ENOENT;
				return -1;
			}
		}

		open_files[next_fd].node = subfile.node;
		if(subfile.node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
		seg = strtok(NULL, PATH_DELIM);
	}

	int return_fd = next_fd;
	if(increment_next_fd() == -1) {
		free(path_copy);
		return -1;
	}
	free(path_copy);
	return return_fd;
}

struct file_entry f_readdir(int fd) {
	struct file_entry subfile;
	subfile.node = -1;
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
		return subfile;
	}

	if (open_files[fd].offset >= cur_disk->inodes[open_files[fd].node].size) {
		errno = ENOENT;
		return subfile;
	}
	int FILE_ENTRY_N = cur_disk->sb.size / FILE_ENTRY_SIZE;
	int block_num = open_files[fd].offset / FILE_ENTRY_N;
	int block_rmd = open_files[fd].offset % FILE_ENTRY_N;

	struct data_block target_block = load_block(open_files[fd].node, block_num);
	printf("%d\n", *((int *) target_block.data));
	printf("%d\n", *((int *) target_block.data+4));
	subfile.node = *((int *) (target_block.data + block_rmd*FILE_ENTRY_SIZE));
	memcpy(subfile.file_name, target_block.data + block_rmd*FILE_ENTRY_SIZE + FILE_INDEX_LENGTH, 
		FILE_NAME_LENGTH);
	free(target_block.data);

	open_files[fd].offset++;
	return subfile;
}

int f_closedir(int fd){
	if (fd > OPEN_FILE_MAX || fd < 0){ //fd overflow
		errno = EBADF;
		return -1;
	}
	if (open_files[fd].node < 0){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{
		if (cur_disk->inodes[open_files[fd].node].type != TYPE_DIRECTORY){ //if fd is not a directory file
			errno = EPERM;
			return -1;
		}
		else{
			open_files[fd].node = -1;
			open_files[fd].offset = 0;
			open_files[fd].mode = -1;
			return 0;
		}
	}
}

int f_mkdir(const char *path, int permission) {
	if (permission < 0) {
		errno = EINVAL;
		return -1;
	}
	char *path_copy = (char *) malloc(FILE_NAME_LENGTH+1);
	bzero(path_copy, FILE_NAME_LENGTH+1);
	strcpy(path_copy, path);
	char *seg = strtok(path_copy, PATH_DELIM);
	if((*path) == PATH_ROOT) { // absolute path
		open_files[next_fd].node = root;
		open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		open_files[next_fd].mode = O_RDONLY;
	} else { // relative path
		open_files[next_fd].node = open_files[pwd_fd].node;
		if(open_files[next_fd].node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
	}

	struct file_entry subfile;
	while(seg) {
		subfile = find_subfile(next_fd, seg);
		if(strend(path, seg)) {
			if(subfile.node < 0) {
				if(create_file(next_fd, seg, permission, TYPE_DIRECTORY) < 0) {
					free(path_copy);
					return -1;
				} else {
					free(path_copy);
					char file_name[FILE_NAME_LENGTH];
					bzero(file_name, FILE_NAME_LENGTH);
					int parent_inode = open_files[next_fd].node;

					subfile = find_subfile(next_fd, seg);
					open_files[next_fd].node = subfile.node;
					open_files[next_fd].offset = 0;
					open_files[next_fd].mode = O_RDONLY;

					file_name[0] = '.';
					if(f_write_helper(&(subfile.node), sizeof(int), 1, next_fd, 
						open_files[next_fd].offset*FILE_ENTRY_SIZE) != sizeof(int)) {
						free(path_copy);
						return -1;
					}
					if(f_write_helper(file_name, FILE_NAME_LENGTH, 1, next_fd, 
						open_files[next_fd].offset*FILE_ENTRY_SIZE+sizeof(int)) != FILE_NAME_LENGTH) {
						free(path_copy);
						return -1;
					}
					open_files[next_fd].offset++;
					cur_disk->inodes[subfile.node].size++;

					file_name[1] = '.';
					if(f_write_helper(&parent_inode, sizeof(int), 1, next_fd, 
						open_files[next_fd].offset*FILE_ENTRY_SIZE) != sizeof(int)) {
						free(path_copy);
						return -1;
					}
					if(f_write_helper(file_name, FILE_NAME_LENGTH, 1, next_fd, 
						open_files[next_fd].offset*FILE_ENTRY_SIZE+sizeof(int)) != FILE_NAME_LENGTH) {
						free(path_copy);
						return -1;
					}
					open_files[next_fd].offset++;
					cur_disk->inodes[subfile.node].size++;
					write_inode(subfile.node);

					return 0;
				}
			} else {
				free(path_copy);
				errno = ENOENT;
				return -1;
			}
		} else {
			if(subfile.node < 0) {
				free(path_copy);
				errno = ENOENT;
				return -1;
			}
		}

		open_files[next_fd].node = subfile.node;
		if(subfile.node == 0)
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
		else
			open_files[next_fd].offset = DIR_INIT_OFFSET;
		open_files[next_fd].mode = O_RDONLY;
		seg = strtok(NULL, PATH_DELIM);
	}
	
	free(path_copy);
	return 0;
}

int f_rmdir(const char *path) {
	int fd = f_opendir(path);
	if(fd > OPEN_FILE_MAX || fd < 0) {
		return -1;
	}
	if(remove_directory(fd) < 0)
		return -1;
	f_closedir(fd);
	return 0;
}

int f_mount(const char *source, const char *target, int flags, void *data) {
	int result;
	int fd = open(source, O_RDWR);
	if(fd == -1) {
		if(format(source, DEFAULT_DISK_SIZE) == -1) { // format fails
			return -1;
		} else {
			fd = open(source, O_RDWR);
			if(fd == -1) {
				return -1;
			}
		}
	}
	if(!target) { // load at the root directory
		if(!disks) { // first disk
			disks = malloc(sizeof(struct disk_image));
			disks->id = 1;
			lseek(fd, OFFSET_SUPERBLOCK, SEEK_SET);
			result = read(fd, &(disks->sb), sizeof(struct superblock));
			if(result == -1) {
				return -1;
			}
			lseek(fd, OFFSET_START + disks->sb.inode_offset*disks->sb.size, SEEK_SET);
			int inode_region_size = (disks->sb.data_offset - disks->sb.inode_offset)*disks->sb.size;
			disks->inodes = (struct inode *) malloc(inode_region_size);
			result = read(fd, disks->inodes, inode_region_size);
			if(result == -1) {
				return -1;
			}
			cur_disk = disks;
			root = 0;
			open_files[next_fd].node = root;
			open_files[next_fd].offset = DIR_INIT_OFFSET-1;
			open_files[next_fd].mode = O_RDONLY;
			for (int i = next_fd+1; i != next_fd; i = (i+1)%OPEN_FILE_MAX){ //initialize the open file table
				open_files[i].node = -1;
				open_files[i].offset = 0;
				open_files[i].mode = -1;
			}

			disks->fd = fd;
			disks->next = 0;
		} else { // not first disk

		}
	} else { // load at the specified location

	}
	return 0;
}

int f_umount(const char *target, int flags) {
	if(!target) { // unmount all mounted disks
		while(disks) {
			struct disk_image *current_disk = disks;
			disks = disks->next;
			close(current_disk->fd);
			free(current_disk->inodes);
			free(current_disk);
			pwd_fd = -1;
		}
	} else { // umount one disk loaded at the specified location
		     // target is the root directory of the disk

	}
	return 0;
}

//****************************HELPER FUNCTIONS******************
size_t f_write_helper(const void *ptr, size_t size, size_t nitems, int fd, int file_offset){
	struct inode * cur_inode = &((cur_disk->inodes)[open_files[fd].node]);
	int BLOCK_SIZE = (cur_disk->sb).size;
	//int N_POINTER = BLOCK_SIZE / sizeof(int);
	size_t rem_size = size * nitems; //remaining read size
	size_t cur_out_offset = 0;
	//if offset is ahead of size, return error
	if (cur_inode -> size < open_files[fd].offset){
		errno = EADDRNOTAVAIL;
		return -1;
	}
	struct data_block temp_data_block;
	int first_block = 0;
	/*if there is offset, need to deal with the first data block to be read*/
	if (file_offset != 0){
		size_t first_block_rem = file_offset % BLOCK_SIZE;
		first_block = file_offset / BLOCK_SIZE;
		if (first_block_rem != 0){
			size_t copy_size = BLOCK_SIZE - first_block_rem;

			if (rem_size < copy_size) copy_size = rem_size;
			temp_data_block = load_block(open_files[fd].node, first_block);
			//if(temp_data_block.data == 0);
				//set errno???
				//return -1;???
			memcpy(temp_data_block.data + first_block_rem, ptr + cur_out_offset, copy_size);
			write_block(open_files[fd].node, first_block, temp_data_block.data);
			free(temp_data_block.data);
			cur_out_offset += copy_size;
			rem_size -= copy_size;
			first_block++;
		}
	}

	int num_block = rem_size / BLOCK_SIZE;
	if (rem_size % BLOCK_SIZE != 0){
		num_block ++;
	}

	size_t remainder;
	/*read from data blocks*/
	for (int i = first_block; i < first_block + num_block; i++){
		if (rem_size <= 0) break;

		remainder = rem_size % BLOCK_SIZE;
		if (remainder == 0){
			remainder += BLOCK_SIZE;
		}

		temp_data_block = load_block(open_files[fd].node, i);
		memcpy(temp_data_block.data, ptr + cur_out_offset, remainder);
		write_block(open_files[fd].node, i, temp_data_block.data);
		free(temp_data_block.data);

		cur_out_offset += remainder;
		rem_size -= remainder;
	}
	
	return nitems * size;
}

int increment_next_fd() {
	int old = next_fd;
	next_fd = (next_fd+1)%OPEN_FILE_MAX;
	while(open_files[next_fd].node >= 0 && next_fd != old) {
		next_fd = (next_fd+1)%OPEN_FILE_MAX;
	}
	if(next_fd == old) {
		errno = ENOMEM;
		return -1;
	} else {
		return 0;
	}
}

int convert_mode(const char* mode) {
	if(strcmp(mode, "r") == 0) {
		return O_RDONLY;
	} else if(strcmp(mode, "w") == 0) {
		return O_WRONLY | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a") == 0) {
		return O_WRONLY | O_CREAT | O_APPEND;
	} else if(strcmp(mode, "r+") == 0) {
		return O_RDWR;
	} else if(strcmp(mode, "w+") == 0) {
		return O_RDWR | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a+") == 0) {
		return O_RDWR | O_CREAT | O_APPEND;
	}
	errno = EINVAL;
	return -1;
}

struct file_entry find_subfile(int dir_fd, char *file_name) {
	struct file_entry subfile;
	subfile.node = -1;
	int old_offset = open_files[dir_fd].offset;

	if(cur_disk->inodes[open_files[dir_fd].node].type != TYPE_DIRECTORY) {
		return subfile;
	}

	int file_size = cur_disk->inodes[open_files[dir_fd].node].size;
	if(open_files[dir_fd].offset < file_size) {
		subfile = f_readdir(dir_fd);
	}
	while(open_files[dir_fd].offset < file_size && strncmp(file_name, subfile.file_name, FILE_NAME_LENGTH) != 0) {
		subfile = f_readdir(dir_fd);
	}
	if(strncmp(file_name, subfile.file_name, FILE_NAME_LENGTH) != 0)
		subfile.node = -1;
	open_files[dir_fd].offset = old_offset;
	return subfile;
}

int create_file(int dir_fd, char *filename, int permission, char type) {
	int new_inode = cur_disk->sb.free_inode;

	// check if more inode could be added
	int INODE_MAX = (cur_disk->sb.data_offset - cur_disk->sb.inode_offset)*cur_disk->sb.size/sizeof(struct inode);
	if(cur_disk->inodes[new_inode].next_inode >= INODE_MAX) {
		errno = ENOSPC;
		return -1;
	}

	cur_disk->sb.free_inode = cur_disk->inodes[cur_disk->sb.free_inode].next_inode;
	write_superblock(); // write superblock back to disk

	cur_disk->inodes[new_inode].next_inode = 0;
	cur_disk->inodes[new_inode].protect = 0;
	cur_disk->inodes[new_inode].parent = open_files[dir_fd].node;
	cur_disk->inodes[new_inode].permission = permission;
	cur_disk->inodes[new_inode].type = type;
	cur_disk->inodes[new_inode].nlink = 1;
	cur_disk->inodes[new_inode].size = 0;
	cur_disk->inodes[new_inode].uid = uid;
	cur_disk->inodes[new_inode].gid = uid;
	for(int i = 0; i < N_DBLOCKS; i++)
		cur_disk->inodes[new_inode].dblocks[i] = 0;
	for(int i = 0; i < N_IBLOCKS; i++)
		cur_disk->inodes[new_inode].iblocks[i] = 0;
	cur_disk->inodes[new_inode].i2block = 0;
	cur_disk->inodes[new_inode].i3block = 0;
	write_inode(new_inode); // write child inode back to disk

	if(f_write_helper(&new_inode, sizeof(int), 1, dir_fd, open_files[dir_fd].offset*FILE_ENTRY_SIZE) != sizeof(int)) {
		return -1;
	}
	if(f_write_helper(filename, FILE_NAME_LENGTH, 1, dir_fd, open_files[dir_fd].offset*FILE_ENTRY_SIZE+sizeof(int)) != FILE_NAME_LENGTH) {
		return -1;
	}

	cur_disk->inodes[open_files[dir_fd].node].size++;
	write_inode(open_files[dir_fd].node); // write parent inode back to disk
	return new_inode;
}

/*assume nlink can be only 0 or 1*/
int remove_file(int dir_fd, int file_inode_idx) {
	/*deal with parent inode*/
	int dir_inode_idx = open_files[dir_fd].node;
	struct inode *dir_inode = &(cur_disk->inodes[dir_inode_idx]);
	if(find_and_remove_entry(dir_fd, file_inode_idx) < 0) {
		errno = ENOENT;
		return -1;
	}
	dir_inode->size--;
	write_inode(dir_inode_idx);

	/* deal with freeblock created (if there is one), didn't change pointer in inode, 
	but change in inode.size by the caller should suffice in not referencing to the freeblock.*/
	int N_FILE_ENTRY = cur_disk->sb.size/FILE_ENTRY_SIZE;
	int last_block_ind = dir_inode->size/N_FILE_ENTRY;
	int last_block_rmd = dir_inode->size % N_FILE_ENTRY;
	if(last_block_rmd == 0) {
		struct data_block temp_block = load_block(dir_inode_idx, last_block_ind);
		add_free_block(temp_block.data_addr);
		free(temp_block.data);
	}

	/*deal with file inode*/
	struct inode *file_inode = &(cur_disk->inodes[file_inode_idx]);
	file_inode->nlink = 0; //change nlink to zero, assume nlink can be only 0 or 1
	clean_all_block(file_inode); //clean all blocks
	file_inode->size = 0;

	file_inode->next_inode = cur_disk->sb.free_inode; //update next free inode
	write_inode(file_inode_idx);

	/*deal with superblock free_inode*/
	cur_disk->sb.free_inode = file_inode_idx;
	write_superblock();

	return 0;
}

/*
1.find file entry and remove it
2.move the last entry (if there is one) to the hole
*/
int find_and_remove_entry(int dir_fd, int file_inode_idx){
	struct file_entry target_subfile, subfile;
	subfile.node = -1;

	if(cur_disk->inodes[open_files[dir_fd].node].type != TYPE_DIRECTORY) {
		return -1;
	}

	if(f_seek(dir_fd, cur_disk->inodes[open_files[dir_fd].node].size-1, SEEK_SET) < 0) {
		return -1;
	}
	target_subfile = f_readdir(dir_fd);

	f_rewind(dir_fd);

	subfile = f_readdir(dir_fd);
	while(subfile.node >=0 && subfile.node != file_inode_idx) {
		subfile = f_readdir(dir_fd);
	}
	if(subfile.node < 0) {
		return -1;
	}
	open_files[dir_fd].offset--;
	if(f_write_helper(&(target_subfile.node), sizeof(int), 1, dir_fd, open_files[dir_fd].offset*FILE_ENTRY_SIZE) != sizeof(int)) {
		return -1;
	}
	if(f_write_helper(target_subfile.file_name, FILE_NAME_LENGTH, 1, dir_fd, open_files[dir_fd].offset*FILE_ENTRY_SIZE+sizeof(int)) != FILE_NAME_LENGTH) {
		return -1;
	}

	return 0;
}

int remove_directory(int dir_fd) {
	struct file_entry subfile;
	subfile.node = 0;
	if(cur_disk->inodes[open_files[dir_fd].node].type != TYPE_DIRECTORY) {
		return -1;
	}
	subfile = f_readdir(dir_fd);
	while(subfile.node >=0) {
		if(subfile.node == open_files[dir_fd].node
			|| subfile.node == cur_disk->inodes[open_files[dir_fd].node].parent) {
		} else if(cur_disk->inodes[subfile.node].type == TYPE_DIRECTORY) {
			open_files[next_fd].node = subfile.node;
			if(subfile.node == 0)
				open_files[next_fd].offset = DIR_INIT_OFFSET-1;
			else
				open_files[next_fd].offset = DIR_INIT_OFFSET;
			open_files[next_fd].mode = O_RDONLY;
			int target_fd = next_fd;
			if(increment_next_fd() < 0) {
				return -1;
			}
			remove_directory(target_fd);
		} else if (cur_disk->inodes[subfile.node].type == TYPE_NORMAL) {
			int subfile_inode_idx = subfile.node;
			remove_file(dir_fd, subfile_inode_idx);
		} else {
			return -1;
		}
		subfile = f_readdir(dir_fd);
	}
	return 0;
}

struct data_block load_block(int node_addr, int block_num) {
	struct inode *node = &(cur_disk->inodes[node_addr]);
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int block_addr;

	// when in direct blocks
	if(block_num < N_DBLOCKS) {
		block_addr = node->dblocks[block_num];
		return load_data_block(block_addr);
	}
	block_num -= N_DBLOCKS;

	// when in indirect blocks
	int i = 0;
	while(block_num >= POINTER_N && i < N_IBLOCKS) {
		block_num -= POINTER_N;
		i++;
	}
	if(i < N_IBLOCKS) {
		return load_indirect_block(node->iblocks[i], block_num);
	}

	// when in i2block
	if(block_num < POINTER_N*POINTER_N) {
		return load_i2block(node->i2block, &block_num);
	}

	block_num -= POINTER_N*POINTER_N;
	// when in i3block
	if(block_num < POINTER_N*POINTER_N*POINTER_N) {
		return load_i3block(node->i3block, &block_num);
	}
	struct data_block null_block;
	null_block.data = 0;
	null_block.data_addr = -1;
	errno = ENOENT;
	return null_block;
}

struct data_block load_data_block(int block_addr) {
	lseek(cur_disk->fd, 
		OFFSET_START + (cur_disk->sb.data_offset + block_addr)*cur_disk->sb.size, 
		SEEK_SET);
	void *data = malloc(cur_disk->sb.size);
	read(cur_disk->fd, data, cur_disk->sb.size);

	struct data_block result;
	result.data = data;
	result.data_addr = block_addr;
	return result;
}

struct data_block load_indirect_block(int iblock_addr, int block_num) {
	struct data_block indirect_block = load_data_block(iblock_addr);
	int block_addr = *((int *) (indirect_block.data + block_num*POINTER_SIZE));
	free(indirect_block.data);
	return load_data_block(block_addr);
}

struct data_block load_i2block(int i2block_addr, int *block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i = 0;
	while((*block_num) >= POINTER_N) {
		(*block_num) -= POINTER_N;
		i++;
	}
	struct data_block i2block = load_data_block(i2block_addr);
	int iblock_addr = *((int *) (i2block.data + i*POINTER_SIZE));
	free(i2block.data);
	return load_indirect_block(iblock_addr, *block_num);
}

struct data_block load_i3block(int i3block_addr, int *block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i = 0;
	while((*block_num) >= POINTER_N*POINTER_N) {
		(*block_num) -= POINTER_N*POINTER_N;
		i++;
	}
	struct data_block i3block = load_data_block(i3block_addr);
	int i2block_addr = *((int *) (i3block.data + i*POINTER_SIZE));
	free(i3block.data);
	return load_i2block(i2block_addr, block_num);
}

/*write data to the block_num th block in inode specified by node_addr*/
int write_block(int node_addr, int block_num, void *data) {
	struct inode *node = &(cur_disk->inodes[node_addr]);
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	size_t file_size = node->size;
	size_t file_block = file_size / cur_disk->sb.size;
	if (file_size % cur_disk->sb.size != 0){
		file_block ++;
	}

	int *add_free;
	if(file_block < block_num+1) {
		add_free = which_to_find_free(file_block, block_num + 1);
	} else {
		add_free = malloc(sizeof(int) * 4);
		bzero(add_free, sizeof(int) * 4);
	}

	// when in direct blocks
	if(block_num < N_DBLOCKS) {
		if (add_free[0]){
			node->dblocks[block_num] = find_free_block();
			write_inode(node_addr);
		}
		write_data_block(node->dblocks[block_num], data);
		free(add_free);
		return 0;
	}

	block_num -= N_DBLOCKS;
	// when in indirect blocks
	int i = 0;
	while(block_num >= POINTER_N && i < N_IBLOCKS) {
		block_num -= POINTER_N;
		i++;
	}
	
	if(i < N_IBLOCKS) {
		if (add_free[1]){
			node->iblocks[i] = find_free_block();
			write_inode(node_addr);
		}
		write_indirect_block(node->iblocks[i], block_num, data, add_free);
		free(add_free);
		return 0;
	}

	// when in i2block
	if(block_num < POINTER_N*POINTER_N) {
		if (add_free[2]){
			node->i2block = find_free_block();
			write_inode(node_addr);
		}
		write_i2block(node->i2block, &block_num, data, add_free);
		free(add_free);
		return 0;
	}

	block_num -= POINTER_N*POINTER_N;
	// when in i3block
	if(block_num < POINTER_N*POINTER_N*POINTER_N) {
		if (add_free[3]){
			node->i3block = find_free_block();
			write_inode(node_addr);
		}
		write_i3block(node->i3block, &block_num, data, add_free);
		free(add_free);
		return 0;
	}
	errno = EFBIG;
	free(add_free);
	return -1;
}

void write_data_block(int block_addr, void * data) {
	lseek(cur_disk->fd, 
		OFFSET_START + (cur_disk->sb.data_offset + block_addr)*cur_disk->sb.size, 
		SEEK_SET);
	write(cur_disk->fd, data, cur_disk->sb.size);
	return;
}

void write_indirect_block(int iblock_addr, int block_num, void * data, int add_free[4]) {
	int block_addr;
	struct data_block indirect_block = load_data_block(iblock_addr);
	if (add_free[0]){ //need to find a free block as data block
		block_addr = find_free_block();
		((int *)indirect_block.data)[block_num] = block_addr;
		write_data(&indirect_block);
	}
	else{
		block_addr = ((int *) indirect_block.data)[block_num];
	}
	free(indirect_block.data);
	write_data_block(block_addr, data);
	return;
}

void write_i2block(int i2block_addr, int *block_num, void * data, int add_free[4]) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	struct data_block i2block = load_data_block(i2block_addr);
	int iblock_addr;
	int i = 0;
	while((*block_num) >= POINTER_N) {
		(*block_num) -= POINTER_N;
		i++;
	}

	if(add_free[1]) {//need to find a free block as 1st level block
		iblock_addr = find_free_block();
		((int *)i2block.data)[i] = iblock_addr;
		write_data(&i2block);
	} else{
		iblock_addr = ((int *) i2block.data)[i];
	}
	free(i2block.data);
	write_indirect_block(iblock_addr, *block_num, data, add_free);
	return;
}

void write_i3block(int i3block_addr, int *block_num, void * data, int add_free[4]) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i2block_addr;
	int i = 0;
	while((*block_num) >= POINTER_N*POINTER_N) {
		(*block_num) -= POINTER_N*POINTER_N;
		i++;
	}
	struct data_block i3block = load_data_block(i3block_addr);

	if (add_free[2]){//need to find a free block as 3rd level block
		i2block_addr = find_free_block();
		((int *)i3block.data)[i] = i2block_addr;
		write_data(&i3block);
	} else{
		i2block_addr = ((int *) i3block.data)[i];
	}
	free(i3block.data);
	write_i2block(i2block_addr, block_num, data, add_free);
	return;
}

/*
return an array of integers, each of which indicates whether we need to find a free block
for entry in the corresponding level block.
*/
int* which_to_find_free (size_t file_block, int block_num){
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int *add_free= malloc(4 * sizeof(int));
	bzero(add_free, 4 * sizeof(int));
	/*if in direct block*/
	if (block_num <= N_DBLOCKS){
		add_free[0] = 1;
	}
	/*if in indirect block*/
	else if (block_num <= N_DBLOCKS + POINTER_N * N_IBLOCKS){
		int j = block_num - N_DBLOCKS;
		int i = 0;
		while(j >= POINTER_N) {
			j -= POINTER_N; 
			i++;
		}
		//i is number of singly indirect block being fully used
		//j is number of data_block in the not fully used block
		add_free[0] = 1;
		if (file_block <= N_DBLOCKS + i * POINTER_N && j == 1){
			add_free[1] = 1;
		}
	}
	/*if in doubly indirect block*/
	else if (block_num <=  N_DBLOCKS + POINTER_N * N_IBLOCKS + POINTER_N * POINTER_N){
		int j = block_num - (N_DBLOCKS + POINTER_N * N_IBLOCKS); // block number relative to the start of i2block
		int i = 0;
		while(j >= POINTER_N) {
			j-= POINTER_N;
			i++;
		}

		//j is now number of fully used 1st level block
		//i is number of remaining data_block
		add_free[0] = 1;
		if (file_block <=  N_DBLOCKS +  N_IBLOCKS * POINTER_N + i * POINTER_N && j == 1){
			add_free[1] = 1;
			if (file_block <=  N_DBLOCKS +  N_IBLOCKS * POINTER_N && i == 0){
				add_free[2] = 1;
			}
		}
	}
	/*if in triply indirect block*/
	else if (block_num <=  N_DBLOCKS + POINTER_N * N_IBLOCKS + POINTER_N * POINTER_N 
		+ POINTER_N * POINTER_N * POINTER_N){
		int total_until_i3 = N_DBLOCKS + POINTER_N * N_IBLOCKS + POINTER_N * POINTER_N;
		int j = block_num - total_until_i3;
		int i = 0;
		while(j >= POINTER_N * POINTER_N) {
			j-= POINTER_N * POINTER_N;
			i++;
		}

		//i is number of 2nd level block being fully used
		//j is now total number of block not in those fully used 2nd level blocks
		int k = j;
		j = 0;
		while (k >= POINTER_N){
			k-= POINTER_N;
			j++;
		}
		//j is total number of 1st level block being fully used
		//k is total number of data block in the not fully used 1st level block
		// i: 2nd; j: 1st; k: data_block
		add_free[0] = 1;
		if (file_block <= total_until_i3 + i * POINTER_N * POINTER_N + j * POINTER_N && k == 1){
			add_free[1] = 1;
			if (file_block <= total_until_i3 + i * POINTER_N * POINTER_N && j == 0){
				add_free[2] = 1;
				if (file_block <= total_until_i3 && i == 0){
					add_free[3] = 1;
				}
			}
		}
	}
	return add_free;
}

void write_superblock() {
	lseek(cur_disk->fd, OFFSET_SUPERBLOCK, SEEK_SET);
	write(cur_disk->fd, &(cur_disk->sb), sizeof(struct superblock));
}

void write_inode(int node) {
	lseek(cur_disk->fd, 
		OFFSET_START + cur_disk->sb.inode_offset*cur_disk->sb.size + node*sizeof(struct inode), 
		SEEK_SET);
	write(cur_disk->fd, &(cur_disk->inodes[node]), sizeof(struct inode));
}

void write_data(struct data_block *db) {
	lseek(cur_disk->fd,
		OFFSET_START + (cur_disk->sb.data_offset + db->data_addr)*cur_disk->sb.size,
		SEEK_SET);
	write(cur_disk->fd, db->data, cur_disk->sb.size);
}

/*
1.removes all data blocks in the given file_inode
2.add them back to free blocks
3.change corresponding datablock pointer in inode to -1
*/
void clean_all_block(struct inode *file_inode){
	size_t rem_size = file_inode->size;
	/*clean direct blocks*/
	for (int i = 0; i < N_DBLOCKS; i++){
		if (rem_size <= 0) break;
		clean_dblock((file_inode->dblocks)[i], &rem_size);
	}
	/*clean indirect blocks*/
	for (int i = 0; i < N_IBLOCKS; i++){
		if (rem_size <= 0) break;
		clean_iblock((file_inode->iblocks)[i], &rem_size);
	}
	/*clean i2block*/
	if (rem_size > 0){
		clean_i2block(file_inode->i2block, &rem_size);
	}
	/*clean i3block*/
	if (rem_size > 0){
		clean_i3block(file_inode->i3block, &rem_size);
	}
}

void clean_dblock(int data_block_addr, size_t *rem_size){
	(*rem_size) -= cur_disk->sb.size;
	add_free_block(data_block_addr);
}

void clean_iblock(int iblock_addr, size_t *rem_size){
	int N_POINTER = cur_disk->sb.size / POINTER_SIZE;
	struct data_block iblock = load_data_block(iblock_addr);
	int data_block_addr;
	for (int i = 0; i < N_POINTER && (*rem_size) > 0; i ++){
		data_block_addr = ((int *)iblock.data)[i];
		clean_dblock(data_block_addr, rem_size);
	}
	add_free_block(iblock_addr);
	free(iblock.data);
}

void clean_i2block(int i2block_addr, size_t *rem_size){
	int N_POINTER = cur_disk->sb.size / POINTER_SIZE;
	struct data_block i2block = load_data_block(i2block_addr);
	int iblock_addr;
	for (int i = 0; i < N_POINTER && (*rem_size) > 0; i ++){
		iblock_addr = ((int *)i2block.data)[i];
		clean_iblock(iblock_addr, rem_size);
	}
	add_free_block(i2block_addr);
	free(i2block.data);
}

void clean_i3block(int i3block_addr, size_t *rem_size){
	int N_POINTER = cur_disk->sb.size / POINTER_SIZE;
	struct data_block i3block = load_data_block(i3block_addr);
	int i2block_addr;
	for (int i = 0; i < N_POINTER && (*rem_size) > 0; i ++){
		i2block_addr = ((int *)i3block.data)[i];
		clean_i2block(i2block_addr, rem_size);
	}
	add_free_block(i3block_addr);
	free(i3block.data);
}

void add_free_block(int block_num){
	int new_head = cur_disk->sb.free_block_head;
	int *temp_free_block = (int *) malloc((cur_disk->sb).size);
 	/*read the free block*/
	lseek(cur_disk->fd,
		OFFSET_START + (cur_disk->sb.free_block_offset + new_head) * (cur_disk->sb).size,
		SEEK_SET);
	read(cur_disk->fd, temp_free_block, (cur_disk->sb).size);
	temp_free_block[temp_free_block[0]+1] = block_num;
	temp_free_block[0]++;

 	/*write back the free block*/
	lseek(cur_disk->fd,
		OFFSET_START + (cur_disk->sb.free_block_offset + new_head) * (cur_disk->sb).size,
		SEEK_SET);
	write(cur_disk->fd, temp_free_block, cur_disk->sb.size);

 	/*move head to the previous block if current block is full*/
	int POINTER_N = cur_disk->sb.size/POINTER_SIZE;
	if (temp_free_block[0] == POINTER_N-1){
		cur_disk->sb.free_block_head--;
		write_superblock();
	}
	free(temp_free_block);
}

int find_free_block(){
	int new_head = (cur_disk->sb).free_block_head;
	int* temp_free_block = (int *) malloc((cur_disk->sb).size);
	int block_num;

 /*read the free block*/
	lseek(cur_disk->fd,
		OFFSET_START + (cur_disk->sb.free_block_offset + new_head) * (cur_disk->sb).size,
		SEEK_SET);
	read(cur_disk->fd, temp_free_block, (cur_disk->sb).size);

	block_num = temp_free_block[temp_free_block[0]];
	temp_free_block[0]--;

	if(temp_free_block[0] == 0) {
		(cur_disk->sb).free_block_head = new_head+1;
	}
	write_superblock();

 /*write back the free block*/
	lseek(cur_disk->fd,
		OFFSET_START + (cur_disk->sb.free_block_offset + new_head) * (cur_disk->sb).size,
		SEEK_SET);
	write(cur_disk->fd, (char*) temp_free_block, cur_disk->sb.size);
	free(temp_free_block);
	return block_num;
}

//****************************UTILITY FUNCTIONS*****************

int strend(const char *s, const char *t) {
    size_t ls = strlen(s); // find length of s
    size_t lt = strlen(t); // find length of t
    if (ls >= lt) { // check if t can fit in s
        // point s to where t should start and compare the strings from there
        return (0 == memcmp(t, s + (ls - lt), lt));
    }
    return 0; // t was longer than s
}

//****************************DEBUG FUNCTIONS*******************
void print_disks() {
	struct disk_image *current = disks;
	while(current) {
		print_disk(current);
		current = current->next;
	}
}

void print_disk(struct disk_image *disk) {
	printf("disk %d\n", disk->id);
	printf("superblock: \n");
	printf("	block size: %d\n", disk->sb.size);
	printf("	inode offset: %d\n", disk->sb.inode_offset);
	printf("	data offset: %d\n", disk->sb.data_offset);
	printf("	free block offset: %d\n", disk->sb.free_block_offset);
	printf(" 	free block head: %d\n", disk->sb.free_block_head);
	printf("	swap offset: %d\n", disk->sb.swap_offset);
	printf("	free inode: %d\n", disk->sb.free_inode);
	struct inode *current = disk->inodes;
	int counter = 0;
	while(((void *) current) < ((void *) disk->inodes) 
		+ (disks->sb.data_offset - disks->sb.inode_offset)*disks->sb.size) {
		printf("inode: %d\n", counter);
		if(current->nlink == 0) {
			printf("	empty inode\n");
			printf("	next inode: %d\n", current->next_inode);
		} else {
			printf("	parent: %d\n", current->parent);
			printf("	permission: %d\n", current->permission);
			printf("	type: %c\n", current->type);
			printf("	nlink: %d\n", current->nlink);
			printf("	size: %d\n", current->size);
			printf("	uid: %d\n", current->uid);
			printf("	gid: %d\n", current->gid);
			printf("	dblocks: ");
			for(int i = 0; i < N_DBLOCKS; i++) {
				printf("%d ", current->dblocks[i]);
			}
			printf("\n");
			printf("	iblocks: ");
			for(int i = 0; i < N_IBLOCKS; i++) {
				printf("%d ", current->iblocks[i]);
			}
			printf("\n");
			printf("	i2block: %d\n", current->i2block);
			printf("	i3block: %d\n", current->i3block);
		}
		current++;
		counter++;
	}
}

void print_fd(int fd) {
	printf("Fd %d in the open file table:\n", fd);
	printf(" 	inode index: %d\n", open_files[fd].node);
	printf(" 	offset: %d\n", open_files[fd].offset);
	printf(" 	mode: %d\n", open_files[fd].mode);
}

void print_file_entry(struct file_entry *fe) {
	printf("File entry\n");
	printf(" 	inode index: %d\n", fe->node);
	printf(" 	file name: ");
	for(int i = 0; i < FILE_NAME_LENGTH; i++) {
		printf("%c", fe->file_name[i]);
	}
	printf("\n");
}

void refresh_open_files() {
	next_fd = 0;
}