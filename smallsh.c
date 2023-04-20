#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>
#include <stddef.h>

pid_t spawnPid = 0;

void handle_SIGINT_action(int signo){
}

// string replace function from youtube video provided by instructor Ryan Gambord
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub, int replaceOnce)
{
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
  sub_len = strlen(sub);   
  for (; (str = strstr(str, needle));) {
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + sub_len - needle_len + 1));
      if (!str) goto exit;
      *haystack = str;
      str = *haystack + off;
    }       memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
      memcpy(str, sub, sub_len);
      haystack_len = haystack_len + sub_len - needle_len;
      str += sub_len;       // if replaceOnce true then break out of loop since only want to replace one occurence
      if (replaceOnce == 1)
      {
        break;
      }
    }
    str = *haystack;
    if (sub_len < needle_len) {
      str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
      if (!str) goto exit;
      *haystack = str;
    } exit:
    return str;
}

int main() {

  struct sigaction SIGINT_action = {0};
  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;
  sigaction(SIGINT, &SIGINT_action, NULL);

  struct sigaction SIGTSTP_action = {0};
  SIGTSTP_action.sa_handler = SIG_IGN;
  sigfillset(&SIGTSTP_action.sa_mask);
  SIGTSTP_action.sa_flags = 0;
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);

  int childStatus =0;
  int exit_status = 0;
  pid_t background_pid = 0;
  int bg_child_status = 0;
  pid_t spawnPid1 = 0;
  int child_status1 = 0;
  int exit_status1 = 0;

  // infinite loop
  while(1){
    
    // variables reset for each new line of input
    char *line = NULL;
    size_t n;
    int i=0;
    int counter = 0;
    int pid = getpid();
    char *home = getenv("HOME");
    size_t len = strlen(home);
    char* home_with_slash = malloc(len+2);
    int input;
    int output;
    int result;
    int background = 0;
    char *delim;

    // set home_with_slash to be expansion of ~/
    for(counter = 0; counter <len; counter++){
      home_with_slash[counter] = home[counter];
    }
    home_with_slash[len+1] = '\0';

    // set delimiter based on IFS or space/tab/newline if IFS is NULL
    if(getenv("IFS")==NULL){
      delim = " \t\n";
    } else{
      delim = getenv("IFS");
    }
    //write(STDERR_FILENO, ps1, ps1_len); // print PS1 at start of line
    
    
    SIGINT_action.sa_handler = handle_SIGINT_action;
    getline(&line, &n, stdin); // get line of text from user
    SIGINT_action.sa_handler = SIG_IGN;

    // break line of text into separate words separated by delimiter (place in arr)
    char *token = strtok(line, delim);
    char* arr[512]; // can have up to 512 words
    char* input_redirect = NULL;
    char* output_redirect = NULL;

    while(token){

      // if token is a #, ignore all following values (as they are a comment)
      if(strcmp(token, "#")==0) {
        break;

      // if token is a &, set background if at end or pre-hashtag  
      } else if(strcmp(token, "&") == 0) {
          background = 1;
          break;

      // input redirection will be needed, set input file    
      } else if (strcmp(token,"<")==0){
        token = strtok(NULL, delim);
        input_redirect = strdup(token);
        token = strtok(NULL, delim);
        continue;

      // output redirection will be needed, set output file  
      } else if(strcmp(token, ">")==0){
        token = strtok(NULL, delim);
        output_redirect = strdup(token);
        token = strtok(NULL, delim);
        continue;
      }
      
      arr[i] = strdup(token); // set token to next parsed word
      
      if(strstr(arr[i], "~/")){  // expansion of ~/ to home
        arr[i] = str_gsub(&arr[i], "~", home, 1);
      }
      if (strstr(arr[i], "$$")){ // expansion of $$ to pid
        char temp[10];
        sprintf(temp, "%d", pid);
        arr[i] = str_gsub(&arr[i], "$$", temp, 1);
      }
      if (strstr(arr[i], "$?")){  // expansion of $?
        char temp[10];
        sprintf(temp, "%d", exit_status);
        arr[i] = str_gsub(&arr[i], "$?", temp, 1);
      }
      if(strstr(arr[i], "$!")){ // expansion of $!
        if(background_pid == 0){
          arr[i] =str_gsub(&arr[i], "$!", "", 1);
        }
        else{
          char temp[10];
          sprintf(temp, "%d", background_pid);
          arr[i] = str_gsub(&arr[i], "$!", temp, 1);
        }
      }
      
      
      token = strtok(NULL, delim); // assign next word to token
      i++; // increment array pointer

    }

    arr[i] = NULL; // set end of array to null
    
    // check for exit command
    if(i>0 && strcmp(arr[0], "exit") == 0){

      int exit_code;
      if(i == 2){
        exit_code = atoi(arr[1]);
      } else {
        exit_code = childStatus; 
      }
      fprintf(stderr, "\nexit\n"); 
      exit(exit_code);
    }

    // check for cd command
    else if (i>0 && strcmp(arr[0], "cd")==0){
      
      if(i == 2){
        if (chdir(arr[1]) == -1){
          printf("Unable to open directory");
          fflush(stdout);
        }
      } else {
        chdir(getenv("HOME"));
      }

    }

    else {    
     // Fork a new process 
     spawnPid = fork();
     switch(spawnPid){
       // case where fork doesn't work
       case -1:
        perror("fork() failed\n");
        exit(1);
        break;

        // child process  
        case 0:
  
          // handle input redirect if specified
          if (input_redirect != NULL){
            input = open(input_redirect, O_RDONLY);
            if (input == -1) {
              perror("Can't open file\n");
              exit(1);
            }
            result = dup2(input, 0);
            if (result == -1) {
              perror("Can't assign input file\n");
              exit(2);
            }
            fcntl(input, F_SETFD, FD_CLOEXEC);
          }

          // handle output redirect if specified
          if(output_redirect != NULL){
            output = open(output_redirect, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (output == -1){
              perror("Can't open file\n");
              exit(1);
            }
            result = dup2(output, 1);
            if (result == -1){
              perror("Unable to assign output file\n");
              exit(2);
            }
            fcntl(output, F_SETFD, FD_CLOEXEC);
          }

          // Execute command
          execvp(arr[0],arr);
          perror("execvp");
          exit(2);

          break;

        //parent process  
        default:            
          // In the parent process
          // Wait for child's termination
          if(background){
            background_pid = spawnPid;
          } else{
            spawnPid = waitpid(spawnPid, &childStatus, 0);
            if (WIFEXITED(childStatus)){
              exit_status = WEXITSTATUS(childStatus);
            }
          }
          break;

     }

  
     while((spawnPid1 =waitpid(0, &bg_child_status, WNOHANG | WUNTRACED)>0)){
       if(WIFSTOPPED(child_status1)){
        fprintf(stderr, "Child process %d stopped. Continuing. \n", spawnPid1);
        fflush(stderr);
        kill(spawnPid1, SIGCONT);
       }
       if(WIFSIGNALED(child_status1)){
        exit_status1 = WTERMSIG(child_status1);
        fprintf(stderr, "Child process %d done. Signaled %d. \n", spawnPid1, exit_status1); 
        fflush(stderr);}
       if(WIFEXITED(child_status1)){
         exit_status1 = WEXITSTATUS(child_status1);
         fprintf(stderr, "Child process %d done. Exit status %d. \n", background_pid, exit_status1);
         fflush(stderr);
       }
     }
  }
  
  }
  return 0;
 }
