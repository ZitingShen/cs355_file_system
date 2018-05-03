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
	struct file_entry subfile;
	int block_num = open_files[fd].offset/(cur_disk->sb.size/FILE_ENTRY_SIZE);
	int block_rmd = open_files[fd].offset % (cur_disk->sb.size/FILE_ENTRY_SIZE);
	if(block_rmd != 0) {
		block_num++;
	}
	void *entry = locate_offset(open_files[fd].node, block_num, block_rmd);
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

void *locate_offset(struct inode *node, int block_num, int block_rmd) {

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