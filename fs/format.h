#ifndef __FORMAT_H__
#define __FORMAT_H__

#ifdef __cplusplus
    extern "C" {
#endif

#define DEFAULT_DISK_SIZE	1024*1024

// when fail, return -1 with proper errno
int format(const char *file_name, int file_size);

#ifdef __cplusplus
}
#endif

#endif