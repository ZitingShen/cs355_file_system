#include "fscalls.h"

extern int pwd_fd;
int mount_or_not = 0;
extern int uid;

#define BLOCK_SIZE 512
#define POINTER_SIZE sizeof(int)
#define FILE_NAME_SIZE 12

using namespace std;

/*add parsing/translation of permission*/
/*deal with user mode*/

bool ls(vector<string> argv){

	/*to do: deal with -F and -l flags*/
	if (mount_or_not <= 0) return false;

	int cur_dir_fd;
	if (argv.size() == 0){
		cur_dir_fd = pwd_fd;
	}
	else{
		string dir_name = argv[0];
		if ((cur_dir_fd = f_open(strdup(dir_name.c_str()), "r")) == -1){
			return false;
		}
	}
	f_rewind(cur_dir_fd);
	
	char * temp_file_entry = (char*) malloc(FILE_ENTRY_SIZE+1);
	while(f_read(temp_file_entry, 1, FILE_ENTRY_SIZE, cur_dir_fd)!=-1){
		printf("%s\t", (char*)temp_file_entry + sizeof(int));
	}
	free(temp_file_entry);

	if (cur_dir_fd != pwd_fd){
		if (f_close(cur_dir_fd)!=0){
			return false;
		}
	}
	
	return true;
}

bool chmod(vector<string> argv){
	if (mount_or_not <= 0) return false;
	if (argv.size() < 2) return false;
	string file_name = argv[0];
	string permission = argv[1];

	if (change_file_mode(file_name.c_str(), strdup(permission.c_str()))!=0){
		return false;
	}
	return true;
}

bool mkdir(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string dir_name = argv[0];
	string permission = argv[1];

	if (f_mkdir(strdup(dir_name.c_str()), convert_mode(strdup(permission.c_str())))!=0){
		return false;
	}
	return true;
}

bool rmdir(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string dir_name = argv[0];

	if (f_rmdir(strdup(dir_name.c_str()))!=0){
		return false;
	}
	return true;
}

bool cd(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string path = argv[0];
	if (f_closedir(pwd_fd)!=0){
		return false;
	}
	int nex_dir = f_opendir(strdup(path.c_str()));
	if (nex_dir < 0){
		return false;
	}
	pwd_fd = nex_dir;
	return true;
}

//assume path name is not longer than 500 char
bool pwd(){
	if (mount_or_not <= 0) return false;

	char* result = get_path(pwd_fd);
	if (!result){
		return false;
	}
	printf("%s\n", result);
	free(result);
	return true;
}

//assume argv is {file 1, file 2 (optional)}
//command is file1 > file2
bool cat(vector<string> argv){
	if (mount_or_not <= 0) return false;

	bool to_std_out = false;
	int fd_out = -1;
	if (argv.size() == 2){
		string out_name = argv[1];
		char* out_name_char = strdup(out_name.c_str());
		if ((fd_out = f_open(out_name_char, "w")) < 0){
			return false;
		}
	}
	else if (argv.size() == 1){
		to_std_out = true;
	}
	else{
		return false;
	}

	string file_name = argv[0];
	char * file_name_char = strdup(file_name.c_str());
	int fd_in;
	if ((fd_in = f_open(file_name_char, "r")) < 0){
		return false;
	}

	size_t rem_size = f_seek(fd_in, 0, SEEK_END);
	f_rewind(fd_in);
	char* buf = (char*) malloc(BLOCK_SIZE+1);

	if (!buf){
		return false;
	}

	int remainder;
	while(rem_size > 0){
		remainder = rem_size % BLOCK_SIZE;
		if (remainder == 0) remainder = BLOCK_SIZE;
		if (f_read(buf, remainder, 1, fd_in)!=0){
			return false;
		}
		if(to_std_out){
			printf("%s\n", buf);
		}
		else{
			if (f_write(buf, remainder, 1, fd_out)!=0){
				free(buf);
				return false;
			}
		}
		rem_size -= remainder;
	}
	free(buf);

	if (f_close(fd_in)!=0){
		return false;
	}

	if (!to_std_out){
		if (f_close(fd_out)!=0)return false;
	}
	
	return true;

}

//does not support redirection
bool more(vector<string> argv){
	if (mount_or_not <= 0) return false;
	struct winsize size;
	if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&size)<0){
      return false;
   	}
   	string in_file_name = argv[0];
   	
   	char* in_file_char = strdup(in_file_name.c_str());
   	
   	int fd_in;
   	if ((fd_in = f_open(in_file_char, "r")) < 0) return false;

	size_t rem_size = f_seek(fd_in, 0, SEEK_END);

	int remainder;
	char* buf = (char*) malloc(size.ws_row+1);

	for (int i = 0; i < size.ws_col; i++){
		if (rem_size <= 0) break;
		remainder = rem_size % size.ws_row;
		if (remainder == 0) remainder = size.ws_row;
		if (f_read(buf, remainder, 1, fd_in)!=0){
			free(buf);
			return false;
		}
		printf("%s\n", buf);
		rem_size -= remainder;
	}

	/*for continuous use*/
	bool cont = true;
	while (cont){
		char* cmd = readline(":");
		if (strcmp(cmd, "q") == 0){
			break;
		}
		else if(strcmp(cmd, "\n") == 0){
			if (rem_size <= 0) continue;
			remainder = rem_size % size.ws_row;
			if (remainder == 0) remainder = size.ws_row;
			if (f_read(buf, remainder, 1, fd_in)!=0){
				free(buf);
				return false;
			}
			printf("%s\n", buf);
			rem_size -= remainder;
		}
	}
	free(buf);

	if (f_close(fd_in)!=0) return false;

	return true;
}

bool rm(vector<string> argv){
	if (mount_or_not <= 0) return false;
	string path = argv[0];
	if (f_remove(path.c_str())!=0){
		return false;
	}
	return true;
}

bool mount(vector<string> argv){
	
	if (argv.size()<2){
		return false;
	}
	string source = argv[0];
	string target = argv[1];

	/*to do: user id??*/

	//find if the disk-image exists
	int fd = open(strdup(source.c_str()), O_RDWR);
	if (fd == -1){ //if disk image does not exist

		char* char_size;
		char_size = readline("Please specify the size of disk you want to format:\n");
		if (char_size == NULL) { /* End of file (ctrl-d) */
      		cout << endl;
      		return false;
    	}
    	int size = atoi(char_size);

    	char* char_name;
		char_name = readline("Please specify the name of disk you want to format:\n");
		if (char_name == NULL) { /* End of file (ctrl-d) */
      		cout << endl;
      		return false;
    	}
    	if (format(char_name, size) != 0){
    		cout << endl;
      		return false;
    	}
    	if (f_mount(char_name, target.c_str(), 0, 0) != 0){
		//mount failed
		return false;
		}
	}
	//if not print prompt
	//display log-in
	//call f mount
	else {
		if (f_mount(source.c_str(), target.c_str(), 0, 0) != 0){
		//mount failed
		close(fd);
		return false;
		}
	}
	printf("%d /n", pwd_fd);
	mount_or_not += 1;
	return true;
	
}

bool umount(vector<string> argv){
	if (mount_or_not <= 0) return false;

	if (argv.size() < 1){
		return false;
	}

	string target = argv[0];

	if (f_umount(target.c_str(), 0) != 0){
		//unmount failed
		return false;
	}
	return true;
}