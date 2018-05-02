#ifndef __UTIL__
#define __UTIL__

#define	POINTER_SIZE		sizeof(int)

#define OFFSET_BOOT			0
#define OFFSET_SUPERBLOCK	512
#define	OFFSET_START		1024

#define N_DBLOCKS 			10
#define N_IBLOCKS 			4

struct superblock {
	int size; /* size of blocks in bytes */
	int inode_offset; /* offset of inode region in blocks */
	int data_offset; /* data region offset in blocks */
	int free_block_offset; /* reserved region to store the indices of free blocks */
	/******************************************************
	For a block reserved to store free block indices, its 
	first int is the number of block indices stored in the 
	block. The rest are all block indices.
	******************************************************/
	int swap_offset; /* swap region offset in blocks */
	int free_inode; /* head of free inode list, index */
};

struct inode {
	int next_inode; /* index of next free inode */
	int protect; /* protection field */

	int parent; /* inode of parent directory */
	int permission;  /* permission */
	char type; /* type of the file */
	/******************************************************
    	d 	Directory.
    	l 	Symbolic link.
    	- 	Regular file.
	******************************************************/
	int nlink; /* number of links to this file */
	int size; /* numer of bytes in file */
	int uid; /* owner’s user ID */
	int gid; /* owner’s group ID */
	/***************************
	0: super user
	-1: omitted argument
	-2: nobody
	https://en.wikipedia.org/wiki/User_identifier
	***************************/
	int ctime; /* change time */
	int mtime; /* modification time */
	int atime; /* access time */

	int dblocks[N_DBLOCKS]; /* pointers to data blocks */
	int iblocks[N_IBLOCKS]; /* pointers to indirect blocks */
	int i2block; /* pointer to doubly indirect block */
	int i3block; /* pointer to triply indirect block */
};

#endif