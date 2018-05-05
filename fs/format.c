#include "util.h"
#include "format.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define BLOCK_SIZE 			512
#define POINTER_N			BLOCK_SIZE/sizeof(int)
#define	FREE_RESERVE_MAX	POINTER_N-1

#define BOOT 		'b'
#define INODE_RATIO	1/100

/******************************************************

Format of an empty disk:
************************
boot block
************************
superblock
************************
inode region
************************
data region
(including free blocks)
************************
reserved region to
store free block indices
************************
swap region
************************

******************************************************/

int format(const char *file_name, int file_size) {
	int fd, out;

	fd = open(file_name, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if(fd == -1) {
		return -1;
	}
	ftruncate(fd, file_size);

	char boot_block[OFFSET_SUPERBLOCK - OFFSET_BOOT];
	memset(&boot_block, BOOT, OFFSET_SUPERBLOCK - OFFSET_BOOT);
	out = write(fd, &boot_block, OFFSET_SUPERBLOCK - OFFSET_BOOT);
	if(out != OFFSET_SUPERBLOCK - OFFSET_BOOT) {
		return -1;
	}

	struct superblock sb;
	sb.size = BLOCK_SIZE;
	sb.inode_offset = 0;
	sb.data_offset = (file_size*INODE_RATIO)/BLOCK_SIZE;

	/******************************************************
	For a block reserved to store free block indices, its 
	first int is the number of block indices stored in the 
	block. The rest are all block indices.
	******************************************************/
	int free_block_num = (file_size - OFFSET_START)/BLOCK_SIZE;
	int free_block_reserve_num = free_block_num/(POINTER_N-1);
	if (free_block_reserve_num % (POINTER_N-1) != 0)
		free_block_reserve_num++;
	sb.free_block_offset = free_block_num - free_block_reserve_num;
	sb.free_block_head = sb.free_block_offset;
	sb.swap_offset = free_block_num;
	sb.free_inode = 1;

	out = write(fd, &sb, sizeof(struct superblock));
	if(out != sizeof(struct superblock)) {
		return -1;
	}

	lseek(fd, OFFSET_START + sb.inode_offset*sb.size, SEEK_SET);
	// write root directory
	struct inode node;
	node.next_inode = 0;
	node.protect = 0;

	node.parent = -1;
	node.permission = PERMISSION_DEFAULT;
	node.type = 'd';
	node.nlink = 1;
	node.size = 0;
	node.uid = 0;
	node.gid = 0;
	for(int i = 0; i < N_DBLOCKS; i++)
		node.dblocks[i] = 0;
	for(int i = 0; i < N_IBLOCKS; i++)
		node.iblocks[i] = 0;
	node.i2block = 0;
	node.i3block = 0;

	out = write(fd, &node, sizeof(struct inode));
	if(out != sizeof(struct inode)) {
		return -1;
	}

	// chain free inodes
	node.nlink = 0;
	int node_index = sb.free_inode;
	int itr;
	for(itr = sb.inode_offset*BLOCK_SIZE; 
		itr <= sb.data_offset*BLOCK_SIZE;
		itr += sizeof(struct inode)) {
		node_index++;
		node.next_inode = node_index;

		out = write(fd, &node, sizeof(struct inode));
		if(out != sizeof(struct inode)) {
			return -1;
		}
	}

	// store free data block indices
	int offset = OFFSET_START + sb.swap_offset*BLOCK_SIZE;
	int counter = free_block_num-1;
	int free_reserve_counter = FREE_RESERVE_MAX;
	while(counter >= 0) {
		offset -= BLOCK_SIZE;
		lseek(fd, offset, SEEK_SET);
		if(counter >= FREE_RESERVE_MAX-1) {
			free_reserve_counter = FREE_RESERVE_MAX;
			out = write(fd, &free_reserve_counter, sizeof(int));
		} else {
			free_reserve_counter = counter+1;
			out = write(fd, &free_reserve_counter, sizeof(int));
		}
		if(out != sizeof(int)) {
			return -1;
		}

		for(int i = 0; i < FREE_RESERVE_MAX && counter >= 0; i++, counter--) {
			out = write(fd, &counter, sizeof(int));
			if(out != sizeof(int)) {
				return -1;
			}
		}
	}
	return 0;
}
