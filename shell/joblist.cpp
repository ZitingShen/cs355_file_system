#include "joblist.h"

using namespace std;

job_t::job_t(int jid, pid_t pid, job_status status, string cmdline, string exec) {
	this->jid = jid;
	this->pid = pid;
	this->status = status;
	this->cmdline = cmdline;
	this->exec = exec;
}

int joblist_t::add(pid_t pid, job_status status, string cmdline, string exec) {
	if(pid < 1) return -1;

	jobs.emplace(jobs.end(), next_jid, pid, status, cmdline, exec);
	next_jid++;
	return 0;
}

int joblist_t::remove_jid(int jid) {
	if(jid < 1) return -1;

	for (list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if (it->jid == jid) {
			remove_helper(it);
			return 0;
		}
	}
	return -1;
}

// remove a job from the joblist
// if there is only one job left, reset next_jid to 1
list<job_t>::iterator joblist_t::remove_helper(list<job_t>::iterator it) {
	if(jobs.size() == 1) {
		next_jid = 1;
	}
	return jobs.erase(it);
}

int joblist_t::remove_pid(pid_t pid) {
	if (pid < 1) return -1;

	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if (it->pid == pid) {
			remove_helper(it);
			return 0;
		}
	}
	return -1;
}

void joblist_t::remove_terminated_jobs() {
	pid_t pid;
	int status;
	for (list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		switch(it->status) {
			case DNBG:
				waitpid(it->pid, &status, 0);
				#if (DEBUG)
				cout << "[" << it->jid << "] (" << it->pid << ")\tDone\t" 
					<< it->cmdline << endl;
				#endif
				it = remove_helper(it);
				break;
			case DNFG:
				#if (DEBUG)
				cout << "[" << it->jid << "] (" << it->pid << ")\tDone\t" 
					<< it->cmdline << endl;
				#endif
				it = remove_helper(it);
				break;
			case TN:
				waitpid(it->pid, &status, 0);
				#if (DEBUG)
				cout << "[" << it->jid << "] (" << it->pid << ")\tTerminated\t" 
					<< it->cmdline << endl;
				#endif
				it = remove_helper(it);
			default:;
		}
	}
	while((pid = waitpid(-1, &status, WNOHANG)) > 1)
		remove_pid(pid);
}

job_t *joblist_t::find_jid(int jid) {
	if(jid < 1) return NULL;

	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->jid == jid) {
			return &(*it);
		}
	}
	return NULL;
}

job_t *joblist_t::find_pid(pid_t pid) {
	if(pid < 1) return NULL;

	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->pid == pid) {
			return &(*it);
		}
	}
	return NULL;
}

job_t *joblist_t::find_exec(string exec) {
	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->exec == exec) {
			return &(*it);
		}
	}
	return NULL;
}

job_t *joblist_t::find_unique_exec(string exec) {
	job_t *result = NULL;
	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->exec == exec) {
			if(result == NULL) {
				result = &(*it);
			} else {
				return NULL;
			}
		}
	}
	return result;
}

job_t *joblist_t::find_stopped() {
	job_t *result = NULL;
	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->status == ST) {
			result = &(*it);
		}
	}
	return result;
}

job_t *joblist_t::find_stopped_or_bg() {
	job_t *result = NULL;
	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->status == ST || it->status == BG) {
			result = &(*it);
		}
	}
	return result;
}

pid_t joblist_t::jid2pid(int jid) {
	if(jid < 1) return -1;

	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->jid == jid) {
			return it->pid;
		}
	}
	return -1;
}

int joblist_t::pid2jid(pid_t pid) {
	if(pid < 1) return -1;

	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		if(it->pid == pid) {
			return it->jid;
		}
	}
	return -1;
}

void joblist_t::listjobs() {
	pid_t pid;
	int status;
	while((pid = waitpid(-1, &status, WNOHANG)) > 1)
		remove_pid(pid);
	for(list<job_t>::iterator it = jobs.begin(); it != jobs.end(); ++it) {
		switch(it->status) {
			case BG:
			case FG:
				cout << '[' << it->jid << "]" << '\t' << "Running\t\t\t"
					 << it->cmdline << endl << flush;
				break;
			case ST:
				cout << '[' << it->jid << "]" << '\t' << "Stopped\t\t\t"
				     << it->cmdline << endl << flush;
				break;
			#if (DEBUG)
			case DNFG:
				cout << '[' << it->jid << "]" << '\t' << "Done (fg)\t\t\t"
					 << it->cmdline << endl << flush;
				break;
			case DNBG:
				cout << '[' << it->jid << "]" << '\t' << "Done (bg)\t\t\t"
					 << it->cmdline << endl << flush;
				break;
			case TN:
				cout << '[' << it->jid << "]" << '\t' << "Terminated\t\t\t"
					 << it->cmdline << endl << flush;
			#endif
			default:;
		}
	}
}
