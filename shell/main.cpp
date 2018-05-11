#include <iostream>

#include "evaluate.h"
#include "handle_signal.h"
#include "joblist.h"
#include "parse.h"
#include "fs.h"

#define shell_terminal STDIN_FILENO

#define ROOT_USERNAME     "root"
#define ROOT_PASSWORD     "rootpwd"
#define DEFAULT_USERNAME  "default"
#define DEFAULT_PASSWORD  "defaultpwd"
#define QUIT              "quit"
#define USER_SUPER        "/usr_super"
#define USER_DEFAULT      "/usr_default"

using namespace std;

struct joblist_t joblist;
struct termios shell_tmodes;
pid_t shell_pid;
int pwd_fd = 0;
int uid = 0;
int mount_or_not = 0;

int main(int argc, char **argv) {
  tcgetattr (shell_terminal, &shell_tmodes);
  shell_pid = getpid();
  char *cmdline;
  bool login_cont = true;
  bool cont = true;

  // register signal handler for SIGCHLD sigaction
  struct sigaction sa_sigchld;
  sa_sigchld.sa_sigaction = &sigchld_handler;
  sigemptyset(&sa_sigchld.sa_mask);
  sa_sigchld.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
  if (sigaction(SIGCHLD, &sa_sigchld, 0) == -1) {
    cerr << "Failed to register SIGCHLD" << endl;
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sigint_handler);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTERM, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  // configure readline to auto-complete paths when the tab key is hit
  rl_bind_key('\t', rl_complete);

  using_history();

  if(f_mount("DISK", 0, 0, 0) < 0) {
    cerr << "Failed to mount the disk" << endl;
    exit(EXIT_FAILURE);
  }
  mount_or_not++;

  cout << "Login" << endl;
  while(login_cont) {
    char *username = readline("username: ");
    char *password = readline("password: ");
    if(username == NULL || password == NULL ) {
      cout << "Failed to login. Try again." << endl;
      cout << "If you want to quit, type 'quit'." << endl;
    } else if(strcmp(username, QUIT) == 0 || strcmp(password, QUIT) == 0) {
      exit(EXIT_FAILURE);
    } else if(strcmp(username, ROOT_USERNAME) == 0 && strcmp(password, ROOT_PASSWORD) == 0) {
      f_mkdir(USER_SUPER, PERMISSION_DEFAULT);
      f_mkdir(USER_DEFAULT, PERMISSION_DEFAULT);
      pwd_fd = f_opendir(USER_SUPER);
      uid = 0;
      login_cont = false;
    } else if(strcmp(username, DEFAULT_USERNAME) == 0 && strcmp(password, DEFAULT_PASSWORD) == 0) {
      f_mkdir(USER_SUPER, PERMISSION_DEFAULT);
      f_mkdir(USER_DEFAULT, PERMISSION_DEFAULT);
      pwd_fd = f_opendir(USER_DEFAULT);
      uid = 1;
      login_cont = false;
    } else {
      cout << "Failed to login. Try again." << endl;
      cout << "If you want to quit, type 'quit'." << endl;
    }
    free(username);
    free(password);
  }
  
  while(cont) {
    joblist.remove_terminated_jobs();
    
    cmdline = readline("Thou shell not crash> ");

    if (cmdline == NULL) { /* End of file (ctrl-d) */
      cout << endl;
      cont = false;
      continue;
    }

    // check for history expansion
    char *output;
    int expansion_result = history_expand(cmdline, &output);

    // If history expansion exists, overwrite cmdline.
    if (expansion_result > 0) {
      strcpy(cmdline, output);
    }
    free(output);

    // If history expansion doesn't exist, print error message.
    if (expansion_result < 0) {
      cerr << cmdline << ": event not found" << endl;
      continue;
    } else if (strcmp(cmdline, "") != 0) {
      add_history(cmdline);

      // semicolon cannot be directly preceded by ampersend
      vector<string> commands;
      if (separate_by_semicolon(cmdline, &commands) < 0) {
        continue;
      }

      for(string command: commands) {
        vector<string> segments;
        if (separate_by_vertical_bar(command, &segments) < 0) {
          continue;
        }

        // when parsing segments, separate <, >, >> from strings 
        // before and after
        vector<vector<string>> parsed_segments = parse_segments(&segments);
        
        // hand processed segments to evaluate
        cont = evaluate(&command, &parsed_segments);
        if (cont == false){
          break;
        }
      }
    }

    free(cmdline);
  }
}
