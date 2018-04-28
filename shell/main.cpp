#include <iostream>

#include "evaluate.h"
#include "handle_signal.h"
#include "joblist.h"
#include "parse.h"
#define shell_terminal STDIN_FILENO


using namespace std;

struct joblist_t joblist;
struct termios shell_tmodes;
pid_t shell_pid;

int main(int argc, char **argv) {
  tcgetattr (shell_terminal, &shell_tmodes);
  shell_pid = getpid();
  char *cmdline;
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
  }
}
