#include "dsh.h"
void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */
void redirection(process_t *job);
job_t* find_job_by_pgid(pid_t pgid, job_t *first_job);
void devilError(const char *message);
pid_t Fork(void);

pid_t dsh_pgid;         /* process group id of dsh */
int dsh_terminal_fd;    /* terminal file descriptor of dsh */
int dsh_is_interactive; /* interactive or batch mode */

/* Initializes the global var jobs_list */
job_t* jobs_list = NULL;

/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
    if (j->pgid < 0) /* first child: use its pid for job pgid */
        j->pgid = p->pid;
    return(setpgid(p->pid,j->pgid));
}

/* Find the job with the given pgid */
job_t* find_job_by_pgid(pid_t pgid, job_t *first_job)
{
  job_t *j = first_job;
  if(!j) return NULL;
  while(j->pgid != pgid && j->next != NULL)
    j = j->next;
  if(j->pgid == pgid)
    return j;
  return NULL;
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
         /* establish a new process group, and put the child in
          * foreground if requested
          */

         /* Put the process into the process group and give the process
          * group the terminal, if appropriate.  This has to be done both by
          * the dsh and in the individual child processes because of
          * potential race conditions.  
          * */

         p->pid = getpid();

         /* also establish child process group in child to avoid race (if parent has not done it yet). */
         set_child_pgid(j, p);

         if(fg){
          seize_tty(j->pgid); // assign the terminal
         } 

         /* Set the handling for job control signals back to the default. */
         signal(SIGTTOU, SIG_DFL);
}

/* Spawning a process with job control. fg is true if the 
 * newly-created process is to be placed in the foreground. 
 * (This implicitly puts the calling process in the background, 
 * so watch out for tty I/O after doing this.) pgid is -1 to 
 * create a new job, in which case the returned pid is also the 
 * pgid of the new job.  Else pgid specifies an existing job's 
 * pgid: this feature is used to start the second or 
 * subsequent processes in a pipeline.
 * */

/*helper method for IO-redirection*/
void redirection(process_t *job){
    if (process -> ifile != NULL) {//input redirection
        int source = open(process -> ifile, O_RDONLY);
        if(source < 0) {
            devilError("Error: fail to open input file");
        }
        else {
            dup2(source, STDIN_FILENO);
            close(source);
        }
    }
    
    if (process -> ofile!=NULL) {//output redirection
        int target = creat(process -> ofile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IRGRP | S_IWOTH | S_IROTH);
        if (target <0) {
            devilError("Error: fail to open or create output file");
        }
        else {
            dup2(target, STDOUT_FILENO);
            close(target);
        }
    }
    
}

//helper function for IO errors
void devilError(const char *message) {
   perror(message);
   exit(1);
}

void spawn_job(job_t *j, bool fg) 
{

	pid_t pid;
	process_t *p;

	for(p = j->first_process; p; p = p->next) {

	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */
	  
	  switch (pid = fork()) {

      case -1: /* fork failure */
        perror("fork");
        exit(EXIT_FAILURE);

      case 0: /* child process  */
        p->pid = getpid();	    
        new_child(j, p, fg);
            
	      /* YOUR CODE HERE?  Child-side code for new process. */
        perror("New child should have done an exec");
        exit(EXIT_FAILURE);  /* NOT REACHED */
        break;    /* NOT REACHED */

      default: /* parent */
            /* establish child process group */
        p->pid = pid;
        set_child_pgid(j, p);

            /* YOUR CODE HERE?  Parent-side code for new process.  */
    }

            /* YOUR CODE HERE?  Parent-side code for new job.*/
	  seize_tty(getpid()); // assign the terminal back to dsh

	}
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j) 
{
     if(kill(-j->pgid, SIGCONT) < 0){
        perror("Error: kill(SIGCONT)");
     }
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.  
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv) 
{

	    /* check whether the cmd is a built in command
        */

        if (!strcmp(argv[0], "quit")) {
            while(jobs_list->next != NULL) {
                delete_job(jobs_list->next, jobs_list);
            }
            delete_job(jobs_list, jobs_list);
            exit(EXIT_SUCCESS);
	}
        else if (!strcmp("jobs", argv[0])) {
            /* Your code here */
            job_t* list_iterator = jobs_list;
            if (list_iterator == NULL) {
                return true;
            }
            int counter = 0;
            while (list_iterator != NULL) {
                counter++;
                printf("[%d]\t %d \t %s\n", counter, list_iterator->pgid, list_iterator->commandinfo);
                list_iterator = list_iterator->next;
            }
            return true;
        }
	else if (!strcmp("cd", argv[0])) {
    /* Your code here */
    if(argv[1] != NULL && argc > 1){ // user specifies directory
      if(chdir(argv[1]) < 0){
        devilError("Error: cd build-in command failed to go to the designated directory");
      }
    } else { // user just types "cd" without a specified directory; go to home directory in default
      if(chdir(getenv("HOME")) < 0){
        devilError("Error: cd build-in command failed to go to HOME directory");
      }
    }
    return true;
  }
  else if (!strcmp("bg", argv[0])) { //optional
    /* Your code here */
    return true;
  }
  else if (!strcmp("fg", argv[0])) {
    /* Your code here */
    job_t* job;
    if(argv[1] != NULL && argc > 1){
      job = find_job_by_pgid((pid_t) atoi(argv[1]), jobs_list);
    } else {
      job = find_last_job(jobs_list);
    }

    if(job != NULL && job->pgid != -1){
      job->bg = false;
      seize_tty(job->pgid);
      continue_job(job);
      //handle_job(job);
      //seize_tty(getpid());
    } else {
      devilError("Error: fg built-in command failed to find a named job");
    }
    return true;
  }
  return false;       /* not a builtin command */
}

/* Build prompt messaage */
char* promptmsg() 
{
    /* Modify this to include pid */
    if(!dsh_is_interactive){ // batch mode
      return ""; // print nothing
    }
    char *prompt = (char *) malloc (sizeof(char) * 50);
    sprintf(prompt, "dsh-%d$: ", getpid());
    return prompt;
}

/* Fork wrapper class: handles error handling */
pid_t Fork(void)
{
  pid_t pid;
  if((pid = fork()) < 0){
    devilError("Fork error");
  }
  return pid;
}
int main() 
{

	init_dsh();
	DEBUG("Successfully initialized\n");
    /* creating dummy job at beginning of list */
    jobs_list = (job_t*) malloc(sizeof(job_t));
    jobs_list->next = NULL;
    jobs_list->commandinfo = NULL;
    jobs_list->first_process = NULL;
    jobs_list->pgid = -1;
    jobs_list->notified = true;
    jobs_list->bg = true;
    jobs_list->mystdin = 0;
    jobs_list->mystdout = 0;
    jobs_list->mystderr = 0;

    while(1) {
        job_t *j = NULL;
		if(!(j = readcmdline(promptmsg()))) {
			if (feof(stdin)) { /* End of file (ctrl-d) */
				fflush(stdout);
				printf("\n");
				exit(EXIT_SUCCESS);
      }
			continue; /* NOOP; user entered return or spaces with return */
		}

        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
        if(PRINT_INFO) print_job(j);

        /* Your code goes here */
        /* You need to loop through jobs list since a command line can contain ;*/
        /* Check for built-in commands */
        /* If not built-in */
            /* If job j runs in foreground */
            /* spawn_job(j,true) */
            /* else */
            /* spawn_job(j,false) */
    }
}
