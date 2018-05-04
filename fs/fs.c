#include "fs.h"
#include "fs_debug.h"
#include "util.h"
#include "format.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

extern int pwd_fd = 0;
struct inode *root = 0;
struct disk_image *cur_disk = 0;
struct disk_image *disks = 0;
struct open_file open_files[OPEN_FILE_MAX];
int next_fd = 0;

int increment_next_fd();
void *load_offset(struct inode *node, int block_num, int block_rmd);
void *load_data_block(int block_addr, int loc);
void *load_indirect_block(int iblock_addr, int block_num, int loc);
void *load_i2block(int i2block_addr, int *block_num, int loc);
void *load_i3block(int i3block_addr, int *block_num, int loc);

//***************************LIBRARY FUNCTIONS******************
int f_open(const char *path, const char *mode) {
	struct inode *target;
	char *seg = strtok(path, PATH_DELIM);
	if(*path == PATH_DELIM) { // absolute path
		open_files[next_fd].node = root;
		open_files[next_fd].offset = 0;
		open_files[next_fd].mode = O_RDONLY;
	} else { // relative path
		open_files[next_fd].node = open_files[pwd_fd].node;
		open_files[next_fd].offset = 0;
		open_files[next_fd].mode = O_RDONLY;
	}

	while(seg) {
		if(open_files[next_fd].node.type != 'd') {
			errno = ENOENT;
			return -1;
		}
		struct file_entry subfile = f_readdir(next_fd);
		while(subfile && strncmp(seg, subfile.file_name, FILE_NAME_LENGTH)) {
			subfile = f_readdir(next_fd);
		}
		if(!subfile) {
			errno = ENOENT;
			return -1;
		}

		open_files[next_fd].node = subfile.node;
		open_files[next_fd].offset = 0;
		open_files[next_fd].mode = O_RDONLY;
		seg = strtok(NULL, PATH_DELIM);
	}

	int return_fd = next_fd;
	if(increment_next_fd() == -1) {
		return -1;
	}
	return return_fd;
}

struct file_entry f_readdir(int fd) {
	// test if the directory can be read
	int FILE_ENTRY_N = cur_disk->sb.size / FILE_ENTRY_SIZE;

	struct file_entry subfile;
	int block_num = open_files[fd].offset / FILE_ENTRY_N;
	int block_rmd = open_files[fd].offset % FILE_ENTRY_N;
	if(block_rmd != 0) {
		block_num++;
	}
	void *target_block = load_block(open_files[fd].node, block_num);
	subfile.node = *((int *) (target_block + block_rmd*FILE_ENTRY_SIZE));
	strncpy(subfile.file_name, target_block + block_rmd*FILE_ENTRY_SIZE + FILE_INDEX_LENGTH, 
		FILE_NAME_LENGTH);
	return subfile;
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
			root = disk->inodes;
			open_files[next_fd].node = root;
			open_files[next_fd].offset = 0;
			open_files[next_fd].mode = O_RDONLY;
			pwd_fd = next_fd;
			if(increment_next_fd() == -1) {
				return -1;
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
			pwd = 0;
		}
	} else { // umount one disk loaded at the specified location
		     // target is the root directory of the disk

	}
	return 0;
}

//****************************HELPER FUNCTIONS******************
int increment_next_fd() {
	int old = next_fd;
	next_fd = (next_fd+1)%OPEN_FILE_MAX;
	while(next_fd.ptr && next_fd != old) {
		next_fd = (next_fd+1)%OPEN_FILE_MAX;
	}
	if(next_fd == old) {
		errno = ENOMEM;
		return -1;
	} else {
		return 0;
	}
}

void *load_block(struct inode *node, int block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int block_addr;

	// when in direct blocks
	if(block_num <= N_DBLOCKS) {
		block_addr = node->dblocks[block_num-1];
		return load_data_block(block_addr);
	}
	block_num -= N_DBLOCKS;

	// when in indirect blocks
	int i = 0;
	while(block_num > POINTER_N && i < N_IBLOCKS) {
		block_num -= POINTER_N;
		i++;
	}
	if(i < N_IBLOCKS) {
		return load_indirect_block(node->iblocks[i], block_num);
	}

	// when in i2block
	if(block_num < POINTER_N*POINTER_N) {
		return load_i2block(node->i2block, block_num);
	}
	block_num -= POINTER_N*POINTER_N;

	// when in i3block
	if(block_num < POINTER_N*POINTER_N*POINTER_N) {
		return load_i3block(node->i3block, block_num);
	}
	errno = ENOENT;
	return -1;
}

void *load_data_block(int block_addr) {
	lseek(cur_disk->fd, 
		OFFSET_START + (cur_disk->sb.data_offset + block_addr)*cur_disk->sb.size, 
		SEEK_SET);
	void *data_block = malloc(cur_disk->sb.size);
	read(cur_disk->fd, data_block, cur_disk->sb.size);
	return data_block;
}

void *load_indirect_block(int iblock_addr, int block_num) {
	int block_addr = *((int *) load_in_block(iblock_addr, 
		block_num*sizeof(int)));
	return load_in_block(block_addr);
}

void *load_i2block(int i2block_addr, int *block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i = 0;
	while((*block_num) > POINTER_N) {
		block_num -= POINTER_N;
		i++;
	}
	int iblock_addr = *((int *) load_in_block(i2block_addr, i*sizeof(int)));
	return load_indirect_block(iblock_addr, block_num);
}

void *load_i3block(int i3block_addr, int *block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i = 0;
	while((*block_num) > POINTER_N*POINTER_N) {
		block_num -= POINTER_N*POINTER_N;
		i++;
	}
	int i2block_addr = *((int *) load_in_block(i3block_addr, i*sizeof(int)));
	return load_indirect_block(i2block_addr, block_num);
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
	printf("	swap offset: %d\n", disk->sb.swap_offset);
	printf("	free inode: %d\n", disk->sb.free_inode);
	struct inode *current = disk->inodes;
	int counter = 0;
	while(((void *) current) < ((void *) disk->inodes) 
		+ (disks->sb.data_offset - disks->sb.inode_offset)*disks->sb.size) {
		printf("inode: %d\n", counter);
		if(current->nlink == 0) {
			printf("	empty block\n");
			printf("	next inode: %d\n", current->next_inode);
		} else {
			printf("	parent: %d\n", current->parent);
			printf("	permission: %d\n", current->permission);
			printf("	type: %c\n", current->type);
			printf("	nlink: %d\n", current->nlink);
			printf("	size: %d\n", current->size);
			printf("	uid: %d\n", current->uid);
			printf("	gid: %d\n", current->gid);
			printf("	ctime: %d\n", current->ctime);
			printf("	mtime: %d\n", current->mtime);
			printf("	atime: %d\n", current->atime);
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