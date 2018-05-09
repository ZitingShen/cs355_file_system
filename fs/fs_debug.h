#ifndef __FS_DEBUG_H__
#define __FS_DEBUG_H__ 

#include "fs.h"

void print_disks();
void print_disk(struct disk_image *disk);
void print_fd(int fd);
void print_file_entry(struct file_entry *fe);
void refresh_open_files();
#endif