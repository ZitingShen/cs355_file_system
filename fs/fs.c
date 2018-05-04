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
extern int uid = 0;
struct inode *root = 0;
struct disk_image *cur_disk = 0;
struct disk_image *disks = 0;
struct open_file open_files[OPEN_FILE_MAX];
int next_fd = 1;

int increment_next_fd();
void set_file_mode(int fd, const char* mode);
void set_inode_mode(int node, const char* mode);
int create_file(int dir_fd, char *filename, const char *mode);

struct data_block load_data_block(int block_addr, int loc);
struct data_block load_indirect_block(int iblock_addr, int block_num, int loc);
struct data_block load_i2block(int i2block_addr, int *block_num, int loc);
struct data_block load_i3block(int i3block_addr, int *block_num, int loc);

void write_superblock();
void write_inode(int node);
void write_data(struct data_block *db);

int strend(const char *s, const char *t);

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
			if(strend(path, seg)) {
				if(create_file(next_fd, seg, mode) == -1)
					return -1;
			} else {
				errno = ENOENT;
				return -1;
			}
		}

		open_files[next_fd].node = subfile.node;
		open_files[next_fd].offset = 0;
		open_files[next_fd].mode = O_RDONLY;
		seg = strtok(NULL, PATH_DELIM);
	}

	set_file_mode(return_fd, mode);
	int return_fd = next_fd;
	if(increment_next_fd() == -1) {
		return -1;
	}
	return return_fd;
}

/*can also close directory file with this function call*/
int f_close(int fd){
	if (fd > OPEN_FILE_MAX){ //fd overflow
		errno = EBADF;
		return EOF;
	}
	if (openfiles[fd] == NULL){ //fd not pointing to a valid inode
		errno = EBADF;
		return EOF;
	}
	else{
		openfiles[fd] = NULL;
		return 0;
	}
}


//success returns zero
int f_seek(int fd, long offset, int whence){
	if (fd > OPEN_FILE_MAX){ //fd overflow
		errno = EBADF;
		return -1;
	}
	if (openfiles[fd] == NULL){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{ //if valid inode stored in openfiles
		long new_offset;
		if (whence == SEEK_SET){
			new_offset = offset;
		}
		else if(whence == SEEK_CUR){
			new_offset = openfiles[fd].offset+ offset;
		}
		else{
			new_offset = ((cur_disk->inodes)[open_files[fd].node]).size + offset;
		}
		open_files[fd].offset = new_offset;
		return new_offset;
	}
}

void f_rewind(int fd){
	if (fd > OPEN_FILE_MAX){ //fd overflow
		errno = EBADF;
	}

	if (openfiles[fd] == NULL){ //fd not pointing to a valid inode
		errno = EBADF;
	}
	else{
		open_files[fd].offset = 0;
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
	
	if (fd > OPEN_FILE_MAX){ //fd overflow
		errno = EBADF;
		return -1;
	}

	if (openfiles[fd] == NULL){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{
		int file_node = openfiles[fd].node;
		int file_size = ((cur_disk -> inodes)[file_node]).size;
		int file_link = ((cur_disk -> inodes)[file_node]).nlink;
		char* file_permission = "";
		int cur_mode = openfiles[fd].mode;

		strcat(file_permission, &(((cur_disk -> inodes)[file_nodenode]).type)); //file type
		if (cur_mode && O_RDONLY){ 
			strcat(file_permission, "r");
		}
		else{
			strcat(file_permission, "-");
		}
		if (cur_mode && O_WRONLY){
			strcat(file_permission, "w");
		}
		else{
			strcat(file_permission, "-");
		}

		printf("File information:\n");
		printf("---------------------------\n");
		printf("File Size:\t\t\t%d\n", file_size);
		printf("Number of Links:\t\t%d\n", file_link);
		printf("File inode:\t\t\t%d\n", file_node);
		printf("File Permissions:\t\t%d\n", file_permission);
		return 0;
	}
}

struct file_entry f_readdir(int fd) {
	if(!(open_files[fd].mode & O_RDONLY)) {
		errno = EACCES;
		return -1;
	}

	int FILE_ENTRY_N = cur_disk->sb.size / FILE_ENTRY_SIZE;

	struct file_entry subfile;
	int block_num = open_files[fd].offset / FILE_ENTRY_N;
	int block_rmd = open_files[fd].offset % FILE_ENTRY_N;
	if(block_rmd != 0) {
		block_num++;
	}
	struct data_block target_block = load_block(open_files[fd].node, block_num);
	subfile.node = *((int *) (target_block.data + block_rmd*FILE_ENTRY_SIZE));
	strncpy(subfile.file_name, target_block.data + block_rmd*FILE_ENTRY_SIZE + FILE_INDEX_LENGTH, 
		FILE_NAME_LENGTH);
	free(target_block.data);

	open_files[fd].offset++;
	cur_disk->inodes[open_files[fd].node].atime = time(0);
	write_inode(open_files[fd].node);
	return subfile;
}

int f_closedir(int fd){
	if (fd > OPEN_FILE_MAX){ //fd overflow
		errno = EBADF;
		return -1;
	}
	if (openfiles[fd] == NULL){ //fd not pointing to a valid inode
		errno = EBADF;
		return -1;
	}
	else{
		struct cur_inode = (cur_disk->inodes)[openfiles[fd].node];
		if (cur_inode.type != 'd'){ //if fd is not a directory file
			errno = EPERM;
			return -1;
		}
		else{
			openfiles[fd] = NULL;
			return 0;
		}
	}
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

void set_file_mode(int fd, const char* mode) {
	if(strcmp(mode, "r") == 0) {
		open_files[fd].mode = O_RDONLY;
	} else if(strcmp(mode, "w") == 0) {
		open_files[fd].mode = O_WRONLY | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a") == 0) {
		open_files[fd].mode = O_WRONLY | O_CREAT | O_APPEND;
	} else if(strcmp(mode, "r+") == 0) {
		open_files[fd].mode = O_RDWR;
	} else if(strcmp(mode, "w+") == 0) {
		open_files[fd].mode = O_RDWR | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a+") == 0) {
		open_files[fd].mode = O_RDWR | O_CREAT | O_APPEND;
	}
}

void set_inode_mode(int node, const char* mode) {
	if(strcmp(mode, "r") == 0) {
		cur_disk->inodes[new_inode].permission = O_RDONLY;
	} else if(strcmp(mode, "w") == 0) {
		cur_disk->inodes[new_inode].permission = O_WRONLY | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a") == 0) {
		cur_disk->inodes[new_inode].permission = O_WRONLY | O_CREAT | O_APPEND;
	} else if(strcmp(mode, "r+") == 0) {
		cur_disk->inodes[new_inode].permission = O_RDWR;
	} else if(strcmp(mode, "w+") == 0) {
		cur_disk->inodes[new_inode].permission = O_RDWR | O_CREAT | O_TRUNC;
	} else if(strcmp(mode, "a+") == 0) {
		cur_disk->inodes[new_inode].permission = O_RDWR | O_CREAT | O_APPEND;
	}
}

int create_file(int dir_fd, char *filename, const char *mode) {
	int new_inode = cur_disk->sb.free_inode;

	// check if more inode could be added
	int INODE_MAX = (cur_disk->sb.data_offset - cur_disk->sb.inode_offset)/sizeof(struct inode);
	if(cur_disk->inodes[cur_disk->sb.free_inode].next_inode >= INODE_MAX) {
		errno = ENOSPC;
		return -1;
	}

	cur_disk->sb.free_inode = cur_disk->inodes[cur_disk->sb.free_inode].next_inode;
	write_superblock(); // write superblock back to disk

	cur_disk->inodes[new_inode].next_inode = 0;
	cur_disk->inodes[new_inode].protect = 0;
	cur_disk->inodes[new_inode].parent = open_files[dir_fd].node;
	set_inode_mode(new_inode, mode);
	cur_disk->inodes[new_inode].type = '-';
	cur_disk->inodes[new_inode].nlink = 1;
	cur_disk->inodes[new_inode].size = 0;
	cur_disk->inodes[new_inode].uid = uid;
	cur_disk->inodes[new_inode].gid = gid;
	cur_disk->inodes[new_inode].ctime = time(0);
	cur_disk->inodes[new_inode].mtime = time(0);
	cur_disk->inodes[new_inode].atime = time(0);
	write_inode(new_inode); // write inode back to disk

	int FILE_ENTRY_N = cur_disk->sb.size / FILE_ENTRY_SIZE;
	int block_num = open_files[dir_fd].offset / FILE_ENTRY_N;
	int block_rmd = open_files[dir_fd].offset % FILE_ENTRY_N;
	if(block_rmd != 0) {
		block_num++;
	}
	struct data_block target_block = load_block(open_files[dir_fd].node, block_num);
	*((int *) (target_block.data + block_rmd*FILE_ENTRY_SIZE)) = new_inode;
	strncpy(target_block.data + block_rmd*FILE_ENTRY_SIZE + FILE_INDEX_LENGTH, filename,
		FILE_NAME_LENGTH);
	write_data(&target_block); // write data back to disk
	free(target_block.data);

	open_files[dir_fd].offset++;
}

struct data_block load_block(int node_addr, int block_num) {
	struct inode *node = cur_disk->inodes[node_addr];
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
	int block_addr = *((int *) load_in_block(iblock_addr, 
		block_num*sizeof(int)));
	return load_data_block(block_addr);
}

struct data_block load_i2block(int i2block_addr, int *block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i = 0;
	while((*block_num) > POINTER_N) {
		block_num -= POINTER_N;
		i++;
	}
	int iblock_addr = *((int *) load_in_block(i2block_addr, i*sizeof(int)));
	return load_indirect_block(iblock_addr, block_num);
}

struct data_block load_i3block(int i3block_addr, int *block_num) {
	int POINTER_N = cur_disk->sb.size / POINTER_SIZE;
	int i = 0;
	while((*block_num) > POINTER_N*POINTER_N) {
		block_num -= POINTER_N*POINTER_N;
		i++;
	}
	int i2block_addr = *((int *) load_in_block(i3block_addr, i*sizeof(int)));
	return load_indirect_block(i2block_addr, block_num);
}

void write_superblock() {
	lseek(cur_disk->fd, OFFSET_SUPERBLOCK, SEEK_SET);
	write(cur_disk->fd, &(cur_disk->sb), sizeof(struct superblock));
}

void write_inode(int node) {
	lseek(cur_disk->fd, 
		OFFSET_START + cur_disk->sb.inode_offset*cur_disk->sb.size, SEEK_SET);
	write(cur_disk->fd, cur_disk->inodes[node], sizeof(struct inode));
}

void write_data(struct data_block *db) {
	lseek(cur_disk->fd,
		OFFSET_START + (cur_disk->sb.data_offset + db->data_addr)*cur_disk->sb.size,
		SEEK_SET);
	write(cur_disk->fd, db->data, cur_disk->sb.size);
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