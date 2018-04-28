#ifndef EVALUATE_H_
#define EVALUATE_H_

#include <signal.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <set>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <sys/wait.h>
#include <readline/history.h>
#include <sys/types.h>
#include <stdio.h>

#include "joblist.h"

using namespace std;

/*
evaluate() takes in command tokens separated by "|", and each token have had ">", "<", ">>" separated from
file names
Step 1: Initialize some intermediate variable to store input and output for piping
Step 2: Create a new child process, H, set the signal handling to default, and create a new process group with N. Depending on whether
		the command is bg or fg(check last character and last token), update joblist with this new process group.
Step 3: if this is child process:
			Loop(for each token separated by "|")
			(1) update intermediate input output variables
			(2) fork a grandchild, G, set the signal handling into default.
				(a)if command is any of: "fg", "bg", "kill", "jobs", "history"(maybe). Preprocess arguments and let 
					grandchild to execute command by calling fg(), bg(), kill(), jobs(), etc.
				(b)if command does not belong to any of that in (a), let grandchild execute the command 
					with execvp()
			(4) wait the grandchild G terminate to continue to the next iteration.
		else: (if parent process)
			wait or donot wait depends on bg/fg

1. if there is no piping, do not fork a process to execute built-in functionality.
2. if there is piping, fork a process to execute each built-in functionality.

return false if the shell should exit, true otherwise.
*/
bool evaluate(string *command, vector<vector<string>> *parsed_segments);

// return false if execvp fails, true otherwise.
bool no_pipe_exec (string *command, vector<string> command_segment, job_status bg_fg);

// return false if execvp fails, true otherwise.
bool pipe_exec(string *command, vector<vector<string>> *parsed_segments, job_status bg_fg, vector<int> *saved_pipes=NULL);

bool built_in_exec(vector<string> argv);

/*
bg() takes in pointer to a list of pids/gpids (as integers). 
For each pid in the list, check whether it is valid pid of a suspended job in joblist (error if not). Then resume
the process by sending SIGCONT. Update job status in joblist accordingly.
*/
bool bg(vector<string> argv);

/*
fg() takes in a integer as pid/gpid of job
Check process status in joblist (if not valid, error), if suspended/stopped, send SIGCONT.
Save shell process status with termio, update status.
Then bring this process to foreground by tcsetpgrp().
Then update status in joblist
*/
bool fg(vector<string> argv);

/*
First check whether pid is valid from joblist.
If flag_set true, send SIGKILL to pid (if child is fg, need to bring shell back to foreground).
else(flag_set false), send SIGTERM to pid (if child is fg, need to bring shell back to foreground).
*/
bool kill(vector<string> argv);

/*
stdout all current jobs by checking joblist. Will go over the entire list within this function.
*/
bool jobs();

/* historyPrint - Print out all the command lines stored in history. If given 
 * an int n, print the last n command lines.
 */
bool history(vector<string> argv);

void handle_error(string exec);
#endif
