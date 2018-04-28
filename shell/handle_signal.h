#ifndef HANDLE_SIGNAL_H_
#define	HANDLE_SIGNAL_H_

#include <csignal>
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <readline/readline.h>

/* SIGCHLD_handler 
	Update job status with si_status.
	*/
void sigchld_handler(int sig, siginfo_t *sip, void *notused); 

/* SIGINT_handler 
	rl_forced_update_display(); to print new prompt
	*/
void sigint_handler(int sig);

#endif

