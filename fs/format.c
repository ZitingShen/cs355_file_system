#include "util.h"
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


void write_triple_indirect_block(int block, struct superblock *sb, int *num_block);
void write_double_indirect_block(int block, struct superblock *sb, int *num_block);
void write_single_indirect_block(int block, struct superblock *sb, int *num_block);
void write_direct_block(int block, struct superblock *sb);

char *file_name = "";
int file_size = 1024*1024;

void parse(int argc, char **argv);

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

int main(int argc, char **argv) {
	int fd, out;

	if(argc != 2 && argc != 4) {
		printf("usage: format <filename> [-s <file size>]\n");
		exit(-1);
	}
	parse(argc, argv);

	fd = open(file_name, O_RDWR | O_CREAT, 0666);
	ftruncate(fd, file_size);

	char boot_block[OFFSET_SUPERBLOCK - OFFSET_BOOT];
	memset(&boot_block, BOOT, OFFSET_SUPERBLOCK - OFFSET_BOOT);
	out = write(fd, &boot_block, OFFSET_SUPERBLOCK - OFFSET_BOOT);
	if(out != OFFSET_SUPERBLOCK - OFFSET_BOOT) {
		printf("%s: %s\n", "Fail to write disk image", strerror(errno));
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
	sb.swap_offset = free_block_num;
	sb.free_inode = 1;

	out = write(fd, &sb, sizeof(struct superblock));
	if(out != sizeof(struct superblock)) {
		printf("%s: %s\n", "Fail to write disk image", strerror(errno));
		return -1;
	}

	// write root directory
	struct inode node;
	node.next_inode = 0;
	node.protect = 0;

	node.parent = -1;
	node.permission = 7;
	node.type = 'd';
	node.nlink = 1;
	node.size = 0;
	node.uid = 0;
	node.gid = 0;
	node.ctime = time(0);
	node.mtime = time(0);
	node.atime = time(0);
	for(int i = 0; i < N_DBLOCKS; i++)
		node.dblocks[i] = 0;
	for(int i = 0; i < N_IBLOCKS; i++)
		node.iblocks[i] = 0;
	node.i2block = 0;
	node.i3block = 0;

	out = write(fd, &node, sizeof(struct inode));
	if(out != sizeof(struct inode)) {
		printf("%s: %s\n", "Fail to write disk image", strerror(errno));
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
			printf("%s: %s\n", "Fail to write disk image", strerror(errno));
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
			printf("%s: %s\n", "Fail to write disk image", strerror(errno));
			return -1;
		}

		for(int i = 0; i < FREE_RESERVE_MAX && counter >= 0; i++, counter--) {
			out = write(fd, &counter, sizeof(int));
			if(out != sizeof(int)) {
				printf("%s: %s\n", "Fail to write disk image", strerror(errno));
				return -1;
			}
		}
	}	
}

void parse(int argc, char **argv) {
	if (argc == 2) {
		file_name = argv[1];
	} else {
		if (strcmp(argv[1], "-s") == 0) {
			int mb = atoi(argv[2]);
			if(mb <= 0) {
				printf("error: invalid file size\n");
				exit(-1);
			}
			file_size *= mb;
			file_name = argv[3];
		} else if (strcmp(argv[2], "-s") == 0) {
			int mb = atoi(argv[3]);
			if(mb <= 0) {
				printf("error: invalid file size\n");
				exit(-1);
			}
			file_size *= mb;
			file_name = argv[1];
		} else {
			printf("usage: format <filename> [-s <file size>]\n");
			exit(-1);
		}
	}
}
