#ifndef __FORMAT_H__
#define __FORMAT_H__

#define DEFAULT_DISK_SIZE	1024*1024

// when fail, return -1 with proper errno
int format(const char *file_name, int file_size);
#endif