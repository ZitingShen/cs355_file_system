#include "fs_calls.h"
using namespace std;

int pwd_fd = 0;
int mount_or_not = 0;
int uid = 0;

#define BLOCK_SIZE 512;
#define POINTER_SIZE sizeof(int)
#define FILE_NAME_SIZE 12
#define FILE_ENTRY_SIZE sizeof(int)+FILE_NAME_SIZE

/*to do*/
char* get_file_name(int inode_idx);
char* find_path(int inode_idx);
int find_inode_by_path(const char * path);
/*add parsing/translation of permission*/
/*deal with user mode*/

bool ls(){
	/*to do: deal with -F and -l flags*/
	if (mount_or_not <= 0) return false;
	char* cur_dir_name = get_file_name(pwd_fd);
	int cur_dir_fd;

	cur_dir_fd = f_open(cur_dir_name, O_RDONLY);
	if (cur_dir_fd == -1){
		return false;
	}

	char[BLOCK_SIZE] buf;
	char[FILE_NAME_SIZE] temp_name;
	while(f_read(buf, 1, BLOCK_SIZE, cur_dir_fd)!=-1){
		for (int i = 0; i < POINTER_SIZE; i+=FILE_ENTRY_SIZE){
			memcpy(temp_name, (char*)buf+i, FILE_NAME_SIZE)
			printf("%s\t", temp_name);
		}
	}

	if (f_close(cur_dir_fd)!=0){
		return false;
	}
	return true;

}

bool mkdir(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string dir_name = argv[0];

	
	string permission = arg[1];
	/*add parsing/translation of permission*/
	int perm_int = atoi(permission);

	if (f_mkdir(dir_name.c_str(), perm_int)!=0){
		return false;
	}
	return true;
}

bool rmdir(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string dir_name = argv[0];
	char dir_name_char[FILE_NAME_SIZE];
	strcpy()
	if (f_rmdir(dir_name.c_str())!=0){
		return false;
	}
	return true;
}


bool cd(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string path = argv[0];
	int nex_dir = find_inode_by_path(path.c_str());
	if (nex_dir < 0){
		return false;
	}
	pwd_fd = nex_dir;
	return true;
}

bool pwd(){
	if (mount_or_not <= 0) return false;
	char* result = find_path(pwd_fd);
	if (!result){
		return false;
	}
	printf("%s\n", pwd_fd);
	return true;
}

bool cat(vector<string> argv){
	if (mount_or_not <= 0) return false;

}

bool more(vector<string> argv){
	if (mount_or_not <= 0) return false;
}

bool rm(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string path = argv[0];
	if (f_remove(source.c_str())!=0){
		return false;
	}
	return true;
}

bool mount(vector<string> argv){
	string source = argv[0];
	string target = argv[1];

	if (!target){
		return false;
	}

	/*to do: user id??*/

	//find if the disk-image exists
	fd = open(source, O_RDWR);
	if (fd == -1){ //if disk image does not exist

		char* char_size;
		char_size = readline("Please specify size of disk you want to format:\n");
		if (char_size == NULL) { /* End of file (ctrl-d) */
      		cout << endl;
      		return false;
    	}
    	int char_size = atoi(cmdline);

    	char* char_name;
		char_name = readline("Please specify size of disk you want to format:\n");
		if (char_name == NULL) { /* End of file (ctrl-d) */
      		cout << endl;
      		return false;
    	}
    	if (format(char_name, char_size) != 0){
    		cout << endl;
      		return false;
    	}
    	if (f_mount(char_name, target.c_str(), 0, 0) != 0){
		//mount failed
		return false;
		}
		mount_or_not += 1;
	}
	//if not print prompt
	//display log-in
	//call f mount
	else (f_mount(source.c_str(), target.c_str(), 0, 0) != 0){
		//mount failed
		return false;
	}
	return true;
	mount_or_not += 1;
}

bool unmount(vector<string> argv){
	if (mount_or_not <= 0) return false;

	string target = argv[0];

	if (!target){
		return false;
	}
	else if (f_unmount(str.c_str(target), 0) != 0){
		//unmount failed
		return false;
	}
	return true;
}