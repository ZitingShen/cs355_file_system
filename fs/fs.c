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

struct disk_image *disks = 0;

int f_open(const char *path, const char *mode) {
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
				printf("%s: %s\n", "Fail to mount disk", strerror(errno));
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
				printf("%s: %s\n", "Fail to read in the disk", strerror(errno));
				return -1;
			}
			lseek(fd, OFFSET_START + disks->sb.inode_offset*disks->sb.size, SEEK_SET);
			int inode_region_size = (disks->sb.data_offset - disks->sb.inode_offset)*disks->sb.size;
			disks->inodes = (struct inode *) malloc(inode_region_size);
			result = read(fd, disks->inodes, inode_region_size);
			if(result == -1) {
				printf("%s: %s\n", "Fail to read in the disk", strerror(errno));
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
		}
	} else { // umount one disk loaded at the specified location
		     // target is the root directory of the disk

	}
	return 0;
}

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