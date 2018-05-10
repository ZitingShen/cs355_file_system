#ifndef __FSCALLS_H__
#define __FSCALLS_H__

#include "fs.h"
#include "format.h"
#include "fs_debug.h"
#include <iostream>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <vector>
#include <sys/ioctl.h>
#include "readline/readline.h"
using namespace std;

//support flags -F and -l
bool ls(vector<string> argv);

bool chmod(vector<string> argv);

bool mkdir(vector<string> argv);

bool rmdir(vector<string> argv);

bool cd(vector<string> argv);

bool pwd();

bool cat(vector<string> argv);

bool more(vector<string> argv);

bool rm(vector<string> argv);

bool mount(vector<string> argv);

bool umount(vector<string> argv);


#endif