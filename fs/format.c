#include "defrag.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define FILE_NAME	"large-frag"
#define	MULTIPLIER	1.1
#define OUTPUT_SIZE (int) (FILE_SIZE*MULTIPLIER)
#define BLOCK_SIZE 	512
#define POINTER_N	BLOCK_SIZE/sizeof(int)
#define	FILE_SIZE 	(POINTER_N*POINTER_N*10 + POINTER_N*N_IBLOCKS + N_DBLOCKS + 1)*BLOCK_SIZE

#define BOOT 		'b'

void write_triple_indirect_block(int block, struct superblock *sb, int *num_block);
void write_double_indirect_block(int block, struct superblock *sb, int *num_block);
void write_single_indirect_block(int block, struct superblock *sb, int *num_block);
void write_direct_block(int block, struct superblock *sb);

void *disk;
int ind = 0;
int next = 0;

int main(int argc, char **argv) {
	disk = malloc(OUTPUT_SIZE);
	if(!disk) {
		printf("%s: %s\n", "Fail to malloc space for the disk image:", strerror(errno));
		return -1;
	}
	bzero(disk, OUTPUT_SIZE);
	memset(disk, BOOT, OFFSET_SUPERBLOCK - OFFSET_BOOT);

	struct superblock sb;
	sb.size = BLOCK_SIZE;
	sb.inode_offset = 0;
	sb.data_offset = 1;
	sb.free_inode = 1;

	struct inode node;
	node.next_inode = 0;
	node.protect = 0;
	node.nlink = 1;
	node.size = FILE_SIZE;
	node.uid = 1;
	node.gid = 1;
	node.ctime = 1;
	node.mtime = 1;
	node.atime = 1;
	for(int i = 0; i < N_DBLOCKS; i++)
		node.dblocks[i] = 0;
	for(int i = 0; i < N_IBLOCKS; i++)
		node.iblocks[i] = 0;
	node.i2block = 0;
	node.i3block = 0;

	int num_block = FILE_SIZE/BLOCK_SIZE;
	if(FILE_SIZE % BLOCK_SIZE != 0)
		num_block++;

	if(num_block > 0) {
		node.i2block = next;
		next++;
		write_double_indirect_block(node.i2block, &sb, &num_block);
	}

	int i = N_IBLOCKS - 1;
	while(num_block > 0 && i >= 0) {
		node.iblocks[i] = next;
		next++;
		write_single_indirect_block(node.iblocks[i], &sb, &num_block);
		i--;
	}


	i = N_DBLOCKS - 1;
	while(num_block > 0 && i >= 0) {
		node.dblocks[i] = next;
		next++;
		write_direct_block(node.dblocks[i], &sb);
		num_block--;
		i--;
	}

	if(num_block > 0) {
		node.i3block = next;
		next++;
		write_triple_indirect_block(node.i3block, &sb, &num_block);
	}

	sb.free_block = next + sb.data_offset;
	sb.swap_offset = next + sb.data_offset;

	int *end = (int *) (disk + OFFSET_START + (sb.data_offset + next)*BLOCK_SIZE);
	*end = -1;

	memcpy(disk + OFFSET_START + sb.inode_offset*BLOCK_SIZE, &node, sizeof(struct inode));
	memcpy(disk + OFFSET_SUPERBLOCK, &sb, sizeof(struct superblock));

	int out_fd = open(FILE_NAME, O_RDWR | O_CREAT, 0666);
	ftruncate(out_fd, OUTPUT_SIZE);
	int out_write = write(out_fd, disk, OUTPUT_SIZE);
	if(out_write != OUTPUT_SIZE) {
		printf("%s: %s\n", "Fail to write disk image", strerror(errno));
		return -1;
	}
	free(disk);
}

void write_triple_indirect_block(int block, struct superblock *sb, int *num_block) {
	int *itr = (int *) (disk + OFFSET_START + (sb->data_offset + block)*BLOCK_SIZE);
	for(int i = 0; i < BLOCK_SIZE/sizeof(int) && *num_block > 0; i++) {
		*itr = next;
		next++;
		write_double_indirect_block(*itr, sb, num_block);
		itr++;
	}
}

void write_double_indirect_block(int block, struct superblock *sb, int *num_block) {
	int *itr = (int *) (disk + OFFSET_START + (sb->data_offset + block)*BLOCK_SIZE);
	for(int i = 0; i < BLOCK_SIZE/sizeof(int)&& *num_block > 0; i++) {
		*itr = next;
		next++;
		write_single_indirect_block(*itr, sb, num_block);
		itr++;
	}
}

void write_single_indirect_block(int block, struct superblock *sb, int *num_block) {
	int *itr = (int *) (disk + OFFSET_START + (sb->data_offset + block)*BLOCK_SIZE);
	for(int i = 0; i < BLOCK_SIZE/sizeof(int)&& *num_block > 0; i++) {
		*itr = next;
		next++;
		write_direct_block(*itr, sb);
		(*num_block)--;
		itr++;
	}
}

void write_direct_block(int block, struct superblock *sb) {
	int *loc = (int *) (disk + OFFSET_START + (sb->data_offset + block)*BLOCK_SIZE);
	for(int i = 0; i < BLOCK_SIZE/sizeof(int); i++) {
		*loc = ind;
		loc++; 
		ind++;
	}
}
