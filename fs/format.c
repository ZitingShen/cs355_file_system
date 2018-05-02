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

#define BLOCK_SIZE 	512
#define POINTER_N	BLOCK_SIZE/sizeof(int)

#define BOOT 		'b'
#define INODE_RATIO	1/100

void write_triple_indirect_block(int block, struct superblock *sb, int *num_block);
void write_double_indirect_block(int block, struct superblock *sb, int *num_block);
void write_single_indirect_block(int block, struct superblock *sb, int *num_block);
void write_direct_block(int block, struct superblock *sb);

void *disk;
int ind = 0;
int next = 0;

char *file_name = "";
int file_size = 1024*1024;

int main(int argc, char **argv) {
	int fd, out;

	if(argc != 2 && argc != 4) {
		printf("usage: format <filename> [-s <file size>]\n");
		exit(-1);
	}
	parse(argc, argv);

	fd = open(file_name, O_RDWR | O_CREAT, 0666);
	ftruncate(fd, file_size);

	disk = malloc(OFFSET_SUPERBLOCK - OFFSET_BOOT);
	if(!disk) {
		printf("%s: %s\n", "Fail to malloc space for the disk image", strerror(errno));
		return -1;
	}
	memset(disk, BOOT, OFFSET_SUPERBLOCK - OFFSET_BOOT);
	out = write(fd, disk, OFFSET_SUPERBLOCK - OFFSET_BOOT);
	if(out != OFFSET_SUPERBLOCK - OFFSET_BOOT) {
		printf("%s: %s\n", "Fail to write disk image", strerror(errno));
		return -1;
	}
	free(disk);

	struct superblock sb;
	sb.size = BLOCK_SIZE;
	sb.inode_offset = 0;
	sb.data_offset = (file_size*INODE_RATIO)/BLOCK_SIZE;
	sb.swap_offset = (file_size - OFFSET_START)/BLOCK_SIZE;
	sb.free_inode = 1;
	sb.free_block = (file_size - OFFSET_START)/BLOCK_SIZE - 1;

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

	// write free data block lists
	int free_current = OFFSET_START + sb.free_inode*BLOCK_SIZE;
	int counter = 0;
	lseek(fd, offset, SEEK_SET);

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
