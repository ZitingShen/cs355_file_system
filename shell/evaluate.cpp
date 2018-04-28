#include "evaluate.h"
#define shell_terminal STDIN_FILENO

extern struct joblist_t joblist;
extern struct termios shell_tmodes;
extern pid_t shell_pid;

using namespace std;

bool evaluate (string *command, vector<vector<string>> *parsed_segments){
	set<string> built_in_commands = {"fg", "bg", "kill", "jobs", "history", "exit"};
	
	bool cont = true;
	int len = parsed_segments->size();
	enum job_status bg_fg = FG; //default as FG
	vector<string> first_seg = (*parsed_segments)[0];

	/* No pipe!!*/
	if (len == 1){
		string cmd = first_seg.front();
		if (built_in_commands.find(cmd) != built_in_commands.end()){ //if first argument is buildin comment
			return built_in_exec(first_seg);
		}
		else{//not built_in
			if (first_seg.back().compare("&") == 0){ //check whether background or foreground
				bg_fg = BG;
				first_seg.pop_back();
			}
			cont = no_pipe_exec(command, first_seg, bg_fg);
		}
	}
	/* Pipe exists!!*/
	else{
		if (parsed_segments->back().back().compare("&") == 0){ //check whether background or foreground
			bg_fg = BG;
			(*parsed_segments)[parsed_segments->size()-1].pop_back();
		}
		cont = pipe_exec(command, parsed_segments, bg_fg);
	}
	return cont;
}


bool no_pipe_exec (string *command, vector<string> argv, job_status bg_fg){
	pid_t pid;
	sigset_t signalSet;  
  	sigemptyset(&signalSet);
  	sigaddset(&signalSet, SIGCHLD);

  	/*Store arguemtns in c strings.*/
  	char** argvc = new char*[argv.size()+1]; 
  	for(unsigned int i = 0; i < argv.size(); i++) {
    	char* temp = new char[argv[i].size()+1];
    	strcpy(temp, argv[i].c_str());
    	argvc[i] = temp;
  	}
  	argvc[argv.size()] = NULL;

  	/* Block SIGCHLD signal while fork(), setpgid and add joblist */  
  	sigprocmask(SIG_BLOCK, &signalSet, NULL);
  	/*fork*/
  	if ((pid = fork()) < 0 ){
		cerr << "Command :" << argv[0] << "Failed to fork child process at process " << getpid() << endl;
	}

	if (pid == 0){
		if (setpgid(0, 0) < 0){
			cerr << "Command :" << argv[0] << "Failed to set new process group" << endl;
		}

		/*unmask SIGCHLD*/
		sigprocmask(SIG_UNBLOCK, &signalSet, NULL);
		
		signal(SIGCHLD, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);

		if (bg_fg == FG){ 
  			tcsetpgrp(shell_terminal, getpid());
  		}

		signal(SIGTTOU, SIG_DFL);

		if (execvp(argvc[0], argvc) < 0){
			handle_error(argv[0]);
			for(unsigned int i = 0; i < argv.size()+1; i++) {
    			delete[] argvc[i];
  			}
  			delete[] argvc;
  			return false;
		}
	}
	else{ //parent process
		/*update joblist*/
		joblist.add(pid, bg_fg, *command, argv[0]);

		if (setpgid(pid, pid) < 0){
			cerr << pid << ": failed to set new group"<<endl;
		}

		if(bg_fg == BG)
			joblist.last_bg = pid;
		/*unmask signals*/
		sigprocmask(SIG_UNBLOCK, &signalSet, NULL);

		//do nothing if bg, will clean up in the next loop.
		if (bg_fg == FG){ //waiting for fg child to complete, need to swap termio, also need to store termio
			//of child if child is stopeed
			tcsetpgrp(shell_terminal, pid);

			int status;
			waitpid(pid, &status, WUNTRACED);

			tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes); // restore shell termio
			tcsetpgrp(shell_terminal, shell_pid); //bring shell to fg

			if (WIFSTOPPED(status)){ //store child termio if stopped
				if (joblist.find_pid(pid)){
					cout<< argv[0] << " stopped"<< endl;
					if (tcgetattr(shell_terminal, &joblist.find_pid(pid)->ter) < 0){
						cerr << "Command :" << argv[0] << "termios of stopped job not saved" << endl; 
					}
				} 
				else {
					cerr << "Command :" << argv[0] << ". "<< pid << " not found in the joblist" << endl;
				}
				
			}
		}
		else{
			if (joblist.pid2jid(pid)<0){
				cerr << "Command :" << argv[0] << ". " << pid << " not found in the joblist" << endl;
			}
			cout << '[' << joblist.pid2jid(pid) << "]\t" << pid << '\t' << *command << endl;
		}
	}

	for(unsigned int i = 0; i < argv.size()+1; i++) {
    	delete[] argvc[i];
  	}
  	delete[] argvc;
  	return true;
}


bool pipe_exec(string *command, vector<vector<string>> *parsed_segments, job_status bg_fg, vector<int> *saved_pipes) {
	unsigned int num_pipes = parsed_segments->size() - 1;
	pid_t pid;
	int pipes[2*num_pipes];
	vector<string> cur_seg;
	set<string> built_in_commands = {"fg", "bg", "kill", "jobs", "history", "exit"};

	if(saved_pipes != NULL) {
		for (unsigned int i = 0; i < num_pipes; i++){
			pipes[i] = (*saved_pipes)[i];
		}
	} else {
		/*create pipe for commands between each | */
		for (unsigned int i = 0; i < num_pipes; i++){ //why do not need to initialize right end pipes???
			if (pipe(&pipes[2*i]) < 0){
				cerr << "Pipe :" << " current pipe: " << 2 * i << " failed to initialize"<< endl;
			}
		}
	}

	sigset_t signalSet;  
  	sigemptyset(&signalSet);
  	sigaddset(&signalSet, SIGCHLD);

	for(unsigned int i = 0; i < parsed_segments->size(); i++) {

		/*Cannot have & before |*/
		cur_seg = (*parsed_segments)[i];
		string cmd = cur_seg[0];

		sigprocmask(SIG_BLOCK, &signalSet, NULL);

		/*fork a child*/
		if ((pid = fork()) < 0 ){
			cerr << "Pipe: Command : " << cmd << ". Failed to fork child process at process." << getpid() << endl;
		}

		if (pid == 0){ //in child process
			if (setpgid(0, 0) < 0){
				cerr << "Pipe: Command : " << cmd << ". Failed to set new process group." << endl;
			}

			/*unmask SIGCHLD*/
			sigprocmask(SIG_UNBLOCK, &signalSet, NULL);
		
			signal(SIGCHLD, SIG_DFL);
			signal(SIGINT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGTTIN, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);

			if (bg_fg == FG){ 
  				tcsetpgrp(shell_terminal, getpid());
  			}

  			signal(SIGTTOU, SIG_DFL);

  			/* If not first command, points fd of stdin to left end of pipe. */
  			if (i != 0){
  				if (dup2(pipes[(i-1) * 2], STDIN_FILENO) < 0){
  					cerr << "Command :" << cmd << ". Left pipe initialization error" << endl;
  					return false;
  				}
  			}

  			/* If not last command, points fd of stdout to right end of pipe. */
      		if (i != num_pipes) {
				if (dup2(pipes[i * 2 + 1], STDOUT_FILENO) < 0) {
	  				cerr << "Command :" << cmd << ". Right pipe initialization error" << endl;
	  				return false;
				}	
      		}

      		/*close pipes*/
      		for (unsigned int i = 0; i < 2 * num_pipes; ++i) {
				close(pipes[i]);
      		}


      		if (built_in_commands.find(cmd) != built_in_commands.end()){ //if first argument is buildin comment
	      		if (cmd.compare("fg") == 0 ){
	      			cerr << "fg: no job control" << endl;
	      			return false;
	      		}
	     		else if (cmd.compare("bg") == 0 ){
	      			cerr << "bg: no job control" << endl;
	      			return false;
	      		} else {
	      			built_in_exec(cur_seg);
	      			return false;
	      		}
	      	}

      		/*Store arguemtns in c strings.*/
  			char** argvc = new char*[cur_seg.size()+1]; 
  			for(unsigned int i = 0; i < cur_seg.size(); i++){
    			char* temp = new char[cur_seg[i].size()+1];
    			strcpy(temp, cur_seg[i].c_str());
    			argvc[i] = temp;
  			}
  			argvc[cur_seg.size()] = NULL;

  			if (execvp(argvc[0], argvc) < 0){
				handle_error(cmd);
				for(unsigned int i = 0; i < cur_seg.size()+1; i++) {
    				delete[] argvc[i];
  				}
  				delete[] argvc;
  				return false;
			}
			return false;
      	} else{ //parent process
			/*update joblist*/
			joblist.add(pid, bg_fg, *command, cmd);
			vector<vector<string>> rest_segs = *parsed_segments;
			rest_segs.erase(rest_segs.begin(), rest_segs.begin()+1);
			job_t *target_job = joblist.find_pid(pid);
			target_job->rest_segs = rest_segs;

			if (setpgid(pid, pid) < 0){
				cerr << "Command :" << cmd << pid << ". failed to set new process group"<<endl;
			}

			if (i == parsed_segments->size()-1 &&bg_fg == BG) 
				joblist.last_bg = pid;
			/*unmask signals*/
			sigprocmask(SIG_UNBLOCK, &signalSet, NULL);

			if (i == parsed_segments->size()-1 && bg_fg == BG){
				cout << '[' << joblist.pid2jid(pid) << "]\t" << pid << '\t' << *command << endl;
			}

			if (bg_fg == FG || (bg_fg == BG && i != parsed_segments->size()-1)) {
				tcsetpgrp(shell_terminal, pid); //bring child to foreground

				int status;
				waitpid(pid, &status, WUNTRACED);

				tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes); // restore shell termio
				tcsetpgrp(shell_terminal, shell_pid); //bring shell to fg

				if (WIFSTOPPED(status)){ //store child termio if stopped
					if (joblist.find_pid(pid)) {
						if (tcgetattr(shell_terminal, &joblist.find_pid(pid)->ter) < 0){
							cerr << "Pipe: Command :" << cmd<< "termios of stopped job not saved" << endl; 
						}
					} 
					else {
						cerr << "Pipe: Command :" << cmd << ". " << pid << " not found in the joblist" << endl;
					}
					return true;
				} else if (WIFSIGNALED(status)) {
					for (unsigned int i = 0; i < num_pipes; i++){
						close(pipes[2*i]);
					}
					return true;
				} else if (i != parsed_segments->size()-1 && WIFEXITED(status)) {
					joblist.remove_pid(pid);
					if(i != 0) close(pipes[2*(i-1)]);
					close(pipes[2*i+1]);
					if(i == parsed_segments->size()-1) close(pipes[2*i]);
				}
			} else {
				close(pipes[2*(i-1)]);
				//close(pipes[2*i+1]);
				//close(pipes[2*i]);
			}
		}
	}
	return true;
}

bool built_in_exec(vector<string> argv) {
	string cmd = argv.front();
	if(cmd.compare("kill") == 0) {
		return kill(argv);
	} else if(cmd.compare("bg") == 0) {
		return bg(argv);
	} else if(cmd.compare("fg") == 0) {
		return fg(argv);
	} else if(cmd.compare("jobs") == 0) {
		return jobs();
	} else if(cmd.compare("history") == 0) {
		return history(argv);
	} else if(cmd.compare("exit") == 0) {
		return false;
	}
	return true;
}

/*
(1) requires % before job number
(2) can take any number of jids
(3) if invalid, will still check the next jid.
(4) -9 flag can only be at the second argument, otherwise -9 will be recognized as a job number
*/
bool kill(vector<string> argv){
	pid_t cur_pid;
	int signo = SIGTERM;
	unsigned int i = 1;
	if (argv.size() < 2){
		cerr << "kill: usage: kill [-sigspec] pid | jobspec ..." << endl;
		return true;
	}

	if (argv[1][0] == '-'){
		try{
			signo = stoi(argv[i].substr(1));
			i++;
		} catch (exception &e) {
		}
	}

	for (;i < argv.size(); i++){
		/* check if pid or jid */
		try {
			if (argv[i][0] == '%'){ //then this is jobid
				if (!joblist.find_jid(stoi(argv[i].substr(1)))){
					cerr << "kill: current: " << argv[i] << " job does not exits" << endl;
					continue;
				}
				cur_pid = joblist.jid2pid(stoi(argv[i].substr(1)));
			}
        	else{ //then is pid
        		cur_pid = stoi(argv[i]);
        		if (!joblist.find_pid(cur_pid)){
        			cerr << "kill: current: "  << argv[i] << " job does not exists" << endl;
        			continue;
        		}
        		
        	}
    	} catch (exception &e){
    		cerr << "kill: "  << argv[i] << " arguments must be process or job IDs" << endl;
    		continue; //continue to the next iteration
   		}

   		
   		//send signo to pid
   		cur_pid = getpgid(cur_pid);//just to double check pgid
   		if(joblist.find_pid(cur_pid)->status == ST) {
   			kill(cur_pid, SIGCONT);
   		}
		if (kill(-cur_pid, signo) > 0){
			cerr << ": job " << "kill: "  << argv[i] << "failed to be killed" << endl;
		}
	}
	return true;
}


/*
(1)resume only when job is ST, otherwise ignore
(2)can take a list of jids, w/ or w/o %
*/
bool bg(vector<string> argv){
	string s_cur_jid;
	int cur_jid;
	job_t *target_job;
	if(argv.size() < 2) {
		if (joblist.last_st){ 
			target_job = joblist.find_pid(joblist.last_st);
			if(target_job != NULL && target_job->status == ST){
				argv.push_back(to_string(target_job->jid));	
			}
		}
		if (argv.size() < 2) {
			cerr << "bg: current: no such job" << endl;
			return true;
		}
	}

	for (unsigned int i = 1; i < argv.size(); i++){
		if ( argv[i][0] == '%') {
      		s_cur_jid = argv[i].substr(1);
      	} 
      	else {
      		s_cur_jid = argv[i];
      	}

		try {
			cur_jid = stoi(s_cur_jid);
			target_job = joblist.find_jid(cur_jid);
      	} catch (exception &e) {
      		if(joblist.find_exec(argv[i])) {
      			target_job = joblist.find_unique_exec(argv[i]);
      			if(!target_job) {
      				cerr << "bg: " << argv[i] << ": ambiguous job spec" << endl;
      				continue;
      			}
      		} else {
      			cerr << "bg: " << argv[i] << ": no such job"<< endl;
    			continue;
      		}
    	}

    	if(!target_job) {
			cerr << "bg: "<< argv[i] << ": no such job" << endl;
			continue;
		}

    	/*only send sigcont when job is ST*/
		if (target_job->status == ST){
			pid_t cur_pid = target_job->pid;

			if (kill(-cur_pid, SIGCONT) < 0){
				cerr << "bg: " << argv[i] << ": failed to continue in background!" << endl;
				continue;
			}
			target_job->status = BG;
			joblist.last_bg = cur_pid;
		}
		else if (target_job->status != BG){
			string err_mes = " ";
			if (target_job->status == TN){
				err_mes = "terminated!";
			}
			else if (target_job->status == DNBG || target_job->status == DNFG){
				err_mes = "been done!";
			}
			cerr << "bg: " << argv[i] << ": job has " << err_mes << endl;
		}
		if(target_job->rest_segs.size() > 0) {
			pipe_exec(&target_job->cmdline, &target_job->rest_segs, BG, &(target_job->saved_pipes));
		}
	}
	return true;
}

/*
(1) take actions only when given a job with ST or BG status.
(2) argument can be any size, but only argv[1] will be take into fg, other will be ignored.
(3) job number can be without % */
bool fg(vector<string> argv){
	int jid;
	job_t *target_job;

	/*check argv size*/
	if(argv.size() < 2) {
		if (joblist.last_bg){ 
			target_job = joblist.find_pid(joblist.last_bg);
			if(target_job != NULL && target_job->status == BG){
				argv.push_back(to_string(target_job->jid));	
			} 
		}

		if (joblist.last_st) {
			target_job = joblist.find_pid(joblist.last_st);
			if(argv.size() < 2 && (target_job != NULL && target_job->status == ST)){
				argv.push_back(to_string(target_job->jid));	
			}
		}

		if (argv.size() < 2) {
			cerr << "fg: current: no such job" << endl;
			return true;
		}
	}

	try { // convert argument to jid
		if (argv[1][0] == '%') {
			jid = stoi(argv[1].substr(1));
		} else {
			jid = stoi(argv[1]);
		}
		target_job = joblist.find_jid(jid);
    } catch (exception &e){
    	if(joblist.find_exec(argv[1])) {
    		target_job = joblist.find_unique_exec(argv[1]);
    		if(!target_job) {
    			cerr << "fg: " << argv[1] << ": ambiguous job spec" << endl;
      			return true;
    		}
    	} else {
    		cerr << "fg: " << argv[1] << ": no such job" << endl;
      		return true;
      	}
    }
	
	if (!target_job){
		cerr << "fg: " << argv[1] << ": no such job" << endl;
		return true;
	}

	/* if job is ST or BG */
	if (target_job->status == ST || target_job->status == BG){
		pid_t pid = target_job->pid;

		if (kill (-pid, SIGCONT) < 0){
			cerr << "fg: " << argv[1] << ": failed to continue when trying to be in foreground" << endl;
			return true;
		}

		target_job->status = BG;

		if (tcsetpgrp(shell_terminal, pid) != 0){ //bring job to fg
			cerr << "fg: " << argv[1] << ": failed to be brought to foreground and will continue in background" << endl;
			return true;
		}

		if (target_job->status == ST){ //reset termio if job stopped
			if (tcsetattr(shell_terminal, TCSADRAIN, &target_job->ter) != 0){
			cerr << "fg: " << argv[1] << ": failed to restore termio setting of current job and will continue in background" << endl;
			tcsetpgrp(shell_terminal, shell_pid); //bring shell to fg
			return true;
			}
		}

		target_job->status = FG;
		int status;

		waitpid(pid, &status, WUNTRACED);

		if (WIFSTOPPED(status)){ //store child termio if stopped
			tcgetattr (shell_terminal, &target_job-> ter); 
		}
		if (tcsetpgrp (shell_terminal, shell_pid) != 0){ //bring shell to foreground
			cerr << "fg: " << argv[1] <<": failed to bring shell to the foreground" << endl;
			return false;
		}
		if (tcsetattr(shell_terminal, TCSADRAIN, &shell_tmodes) != 0){ // restore shell termio
			cerr << "fg: " << argv[1] <<": failed to restore shell termio setting" << endl;
			return false;
		}

		if(target_job->rest_segs.size() > 0) {
			pipe_exec(&target_job->cmdline, &target_job->rest_segs, FG, &(target_job->saved_pipes));
		}
	}

	/* if job is not ST or BG, cerr */
	else{
		string err_mes = " ";
		if (target_job->status == TN){
			err_mes = "terminated!";
		}
		else if (target_job->status == DNBG || target_job->status == DNFG){
			err_mes = "been done!";
		}
		cerr << "fg: " << argv[1] << ": job has " << err_mes << "." << endl;
	}
	return true;
}


bool jobs(){
	joblist.listjobs();
	return true;
}

bool history(vector<string> argv) {
	int n = history_length;
	if (argv.size() > 2) {
		cout << "history: too many arguments" << endl;
		return true;
	}
	if (argv.size() == 2) {
		try {
			n = stoi(argv[1]);
		} catch(exception &e) {
			cout << "Usage: history [n]" << endl;
			return true;
		}
		if (n < 0) {
			cout << "Usage: history [n]" << endl;
			return true;
		}
		if (n > history_length) n = history_length;
	}
	for (int i = history_base + history_length - n; 
		i < history_base + history_length; i++) {
		cout <<  i << " " << history_get(i)->line << endl;
	}
	return true;
}

void handle_error(string exec) {
	switch(errno) {
		case E2BIG:
			cerr << exec << ": argument list too long" << endl;
			break;
		case EACCES:
			cerr << exec << ": permission denied" << endl;
			break;
		case EAGAIN:
			cerr << exec << ": resource temporarily unavailable" << endl;
			break;
		case EFAULT:
			cerr << exec << ": bad address" << endl;
			break;
		case EINTR:
			cerr << exec << ": interrupted function call" << endl;
			break;
		case ELOOP:
			cerr << exec << ": too many levels of symbolic links" << endl;
			break;
		case ENAMETOOLONG:
			cerr << exec << ": filename too long" << endl;
			break;
		case ENOENT:
			cerr << exec << ": command not found" << endl;
			break;
		case ENOLINK:
			cerr << exec << ": link has been severed" << endl;
			break;
		case ENOTDIR:
			cerr << exec << ": not a directory" << endl;
			break;
		case ENOEXEC:
			cerr << exec << ": exec format error" << endl;
			break;
		case ENOMEM:
			cerr << exec << ": not enough space/cannot allocate memory" << endl;
			break;
		case ETXTBSY:
			cerr << exec << ": text file busy" << endl;
			break;
		default:
			cerr << exec << ": error" << endl;
			break;
	}
}

