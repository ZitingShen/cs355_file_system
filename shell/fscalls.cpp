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
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}

	int cur_dir_fd;
	if (argv.size() == 1){
		cur_dir_fd = pwd_fd;
	}
	else{
		string dir_name = argv[1];
		char *dir_name_copy = strdup(dir_name.c_str());
		if ((cur_dir_fd = f_opendir(dir_name_copy)) < 0) {
			cerr << "error: fail to open the file" << endl;
			free(dir_name_copy);
			return true;
		}
		free(dir_name_copy);
	}
	f_rewind(cur_dir_fd);
	
	struct file_entry temp_file_entry = f_readdir(cur_dir_fd);
	while(temp_file_entry.node >= 0){
		printf("%s\t", temp_file_entry.file_name);
		temp_file_entry = f_readdir(cur_dir_fd);
	}
	cout << endl;

	if (cur_dir_fd != pwd_fd){
		if (f_close(cur_dir_fd) != 0){
			cerr << "error: fail to close the file" << endl;
			return true;
		}
	}
	
	return true;
}

bool chmod(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}

	if (argv.size() < 3) {
		cerr << "usage: chmod <file path> <permission>" << endl;
		return true;
	}
	string file_name = argv[1];
	string permission = argv[2];

	if (change_file_mode(file_name.c_str(), strdup(permission.c_str()))!=0){
		cerr << "error: change mode failure" << endl;
		return true;
	}
	return true;
}

bool mkdir(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}

	if(argv.size() < 2) {
		cerr << "usage: mkdir <directory path>" << endl;
		return true;
	}

	string dir_name = argv[1];

	char *dir_name_copy = strdup(dir_name.c_str());
	if (f_mkdir(dir_name_copy, PERMISSION_DEFAULT) < 0) {
		cerr << "error: fail to make the directory" << endl;
		free(dir_name_copy);
		return true;
	}
	free(dir_name_copy);
	return true;
}

bool rmdir(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}

	if(argv.size() < 2) {
		cerr << "usage: rmdir <directory path>" << endl;
		return true;
	}

	string dir_name = argv[1];

	char *dir_name_copy = strdup(dir_name.c_str());
	if (f_rmdir(dir_name_copy) != 0) {
		cerr << "error: fail to remove the directory" << endl;
		free(dir_name_copy);
		return true;
	}
	free(dir_name_copy);
	return true;
}

bool cd(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}
	if(argv.size() < 2) {
		cerr << "usage: cd <path>" << endl;
		return true;
	}

	string path = argv[1];
	char *path_copy = strdup(path.c_str());
	if(path == "." || path == "..") {
		f_seek(pwd_fd, 0, SEEK_SET);
	}
	int nex_dir = f_opendir(path_copy);
	if (nex_dir < 0){
		cerr << "fail to open the directory" << endl;
		free(path_copy);
		return true;
	}
	if (f_closedir(pwd_fd) != 0){
		cerr << "fail to close the directory" << endl;
		return true;
	}
	free(path_copy);
	pwd_fd = nex_dir;
	return true;
}

//assume path name is not longer than 500 char
bool pwd(){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}

	char* result = get_path(pwd_fd);
	if (!result){
		cerr << "error: pwd failure" << endl;
		return true;
	}
	printf("%s\n", result);
	free(result);
	return true;
}

//assume argv is {file 1, file 2 (optional)}
//command is file1 > file2
bool cat(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}

	bool to_std_out = false;
	int fd_out = -1;
	if (argv.size() == 3 && argv[1] == ">"){
		string out_name = argv[2];
		char* out_name_char = strdup(out_name.c_str());
		if ((fd_out = f_open(out_name_char, "w")) < 0) {
			cerr << "fail to open the file" << endl;
			return true;
		}
	} else if (argv.size() == 1){
		to_std_out = true;
	} else{
		cerr << "usage: cat <input file> > <output file>" << endl;
		return true;
	}

	string file_name = argv[1];
	char * file_name_char = strdup(file_name.c_str());
	int fd_in;
	if ((fd_in = f_open(file_name_char, "r")) < 0){
		cerr << "error: fail to open the file" << endl;
		return true;
	}

	size_t rem_size = f_seek(fd_in, 0, SEEK_END);
	f_rewind(fd_in);
	char* buf = (char*) malloc(BLOCK_SIZE+1);

	if (!buf){
		cerr << "error: fail to malloc" << endl;
		return true;
	}

	int remainder;
	while(rem_size > 0){
		remainder = rem_size % BLOCK_SIZE;
		if (remainder == 0) remainder = BLOCK_SIZE;
		if (f_read(buf, remainder, 1, fd_in)!=0) {
			cerr << "error: fail to read in the file" << endl;
			return true;
		}
		if(to_std_out){
			printf("%s\n", buf);
		}
		else{
			if (f_write(buf, remainder, 1, fd_out)!=0){
				free(buf);
				cerr << "error: fail to write into the file" << endl;
				return true;
			}
		}
		rem_size -= remainder;
	}
	free(buf);

	if (f_close(fd_in)!=0) {
		cerr << "error: fail to close the file" << endl;
		return true;
	}

	if (!to_std_out){
		if (f_close(fd_out)!=0) {
			cerr << "error: fail to close the file" << endl;
			return true;
		}
	}
	
	return true;

}

//does not support redirection
bool more(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}
	if (argv.size() < 2) {
		cerr << "usage: more <filename>" << endl;
		return true;
	}
	struct winsize size;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) < 0){
		cerr << "error: fail to get the screen size" << endl;
    	return true;
   	}

   	string in_file_name = argv[1];
   	
   	char* in_file_char = strdup(in_file_name.c_str());
   	
   	int fd_in;
   	if ((fd_in = f_open(in_file_char, "r")) < 0)  {
   		cerr << "error: fail to read file" << endl;
   		return true;
   	}

	size_t rem_size = f_seek(fd_in, 0, SEEK_END);

	int remainder;
	char* buf = (char*) malloc(size.ws_row+1);

	for (int i = 0; i < size.ws_col; i++){
		if (rem_size <= 0) break;
		remainder = rem_size % size.ws_row;
		if (remainder == 0) remainder = size.ws_row;
		if (f_read(buf, remainder, 1, fd_in)!=0){
			free(buf);
			return true;
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
				return true;
			}
			printf("%s\n", buf);
			rem_size -= remainder;
		}
	}
	free(buf);

	if (f_close(fd_in)!=0) {
		cerr << "error: fail to close file" << endl;
		return true;
	}

	return true;
}

bool rm(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	}
	if(argv.size() < 2) {
		cerr << "usage: rm <filename>" << endl;
		return true;
	}
	string path = argv[1];
	if (f_remove(path.c_str()) != 0){
		cerr << "error: remove failure" << endl;
		return true;
	}
	return true;
}

bool mount(vector<string> argv){
	if (argv.size() < 2){
		cerr << "usage: mount <source file> [target location]" << endl;
		return true;
	}

	/*to do: user id??*/

	//if not print prompt
	//display log-in
	//call f mount
	if(argv.size() == 2) {
		string source = argv[1];
		if (f_mount(source.c_str(), 0, 0, 0) != 0){
			//mount failed
			cerr << "error: mount failure" << endl;
			return true;
		}
	} else {
		string source = argv[1];
		string target = argv[2];
		if (f_mount(source.c_str(), target.c_str(), 0, 0) != 0){
			//mount failed
			cerr << "error: mount failure" << endl;
			return true;
		}
	}
	
	pwd_fd = f_opendir("/");
	mount_or_not++;
	return true;
	
}

bool umount(vector<string> argv){
	if (mount_or_not <= 0) {
		cerr << "error: no mounted disk" << endl;
		return true;
	} 

	if(argv.size() == 2) {
		string target = argv[1];
		if (f_umount(target.c_str(), 0) != 0){
			//unmount failed
			cerr << "error: umount failure" << endl;
			return true;
		} else {
			mount_or_not--;
		}
	} else {
		if (f_umount(0, 0) != 0){
			//unmount failed
			cerr << "error: umount failure" << endl;
			return true;
		} else {
			mount_or_not--;
		}
	}
	return true;
}