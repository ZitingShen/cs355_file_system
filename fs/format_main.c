#include "format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *parse(int argc, char **argv, int *file_size);

int main(int argc, char **argv) {
	if(argc != 2 && argc != 4) {
		printf("usage: format <filename> [-s <file size>]\n");
		exit(-1);
	}

	int file_size = DEFAULT_DISK_SIZE;
	char *file_name = parse(argc, argv, &file_size);
	if(strcmp(file_name, "") != 0)
		format(file_name, file_size);
}

char *parse(int argc, char **argv, int *file_size) {
	char *file_name = "";
	if (argc == 2) {
		file_name = argv[1];
	} else {
		if (strcmp(argv[1], "-s") == 0) {
			int mb = atoi(argv[2]);
			if(mb <= 0) {
				printf("error: invalid file size\n");
				return "";
			}
			*file_size *= mb;
			file_name = argv[3];
		} else if (strcmp(argv[2], "-s") == 0) {
			int mb = atoi(argv[3]);
			if(mb <= 0) {
				printf("error: invalid file size\n");
				return "";
			}
			*file_size *= mb;
			file_name = argv[1];
		} else {
			printf("usage: format <filename> [-s <file size>]\n");
			return "";
		}
	}
	return file_name;
}