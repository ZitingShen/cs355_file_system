#include "handle_signal.h"
#include "joblist.h" 

#define shell_terminal STDIN_FILENO

using namespace std;

extern struct joblist_t joblist;
extern struct termios shell_tmodes;

void sigchld_handler(int sig, siginfo_t *sip, void *notused){
	//int exit_status = sip->si_status;
	int exit_code = sip->si_code;
	pid_t pid = sip->si_pid;
	job_t *target_job = joblist.find_pid(pid);
	if (exit_code == CLD_EXITED){
		if(target_job->status == BG) {
			//cout << "DNBG" << endl;
			target_job->status = DNBG;
		}
		else{
			//cout << "DNFG" << endl;
			target_job->status = DNFG;
		}
	} else if (exit_code == CLD_KILLED) {
		//cout << "TN" << endl;
		target_job->status = TN;
	} else if (exit_code == CLD_STOPPED) {
		//cout << "ST" << endl;
		target_job->status = ST;
		joblist.last_st = pid;
		//cout << "[" << joblist.pid2jid(pid) << "]" << "] (" << pid << ")\tStopped\t\tSignal " << WSTOPSIG(exit_status
			//) << endl;
		cout << "[" << target_job->jid << "] (" << pid << ")\tStopped\t\t" << target_job->cmdline << endl;
	}
	//cout << pid << "\t" << exit_code << endl;
}

void sigint_handler(int sig){
	cout << endl;
	rl_forced_update_display();
}
