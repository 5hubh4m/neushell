#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <signal.h>
#include <ncurses.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#define  DEF_PROMPT "neushell>"
#define  INPUT_SIZE 1024
#define  ARG_SIZE 50
#define  ESC_TOKENS "\"\'"
#define  SEP_TOKENS " \t\n"
#define  END_TOKENS "\0\r"

char     defpath[INPUT_SIZE + 1];
char     pwd[MAXPATHLEN + 1];

void     getinput(char*);
void     hndlsigcc(int);
bool     validate(char, char*);
int      makeargs(char*, char*[]);
int      redirect(char*[]);
int      pipecmd(char*[], int*);
void     execute(int, char*[]);

int main(int argc, char* argv[]) {
  char input[INPUT_SIZE + 1];
  char* args[ARG_SIZE];
  int nargs, ORIG_STDIN, ORIG_STDOUT;
  signal(SIGINT, SIG_IGN);
	signal(SIGINT, hndlsigcc);
  ORIG_STDOUT = dup(STDOUT_FILENO);
  ORIG_STDIN = dup(STDIN_FILENO);
  getwd(pwd);
  strcpy(defpath, getenv("PATH"));
  char* pathvar = malloc(strlen(defpath) + strlen(pwd) + 2);
  strcpy(pathvar, pwd);
  strcat(pathvar, ":");
  strcat(pathvar, defpath);
  setenv("PATH", pathvar, 1);
  free(pathvar);
  setenv("PROMPT", DEF_PROMPT, 0);
  while(true) {
    printf("%s ", getenv("PROMPT"));
    fgets(input, INPUT_SIZE + 1, stdin);
    nargs = makeargs(input, args);
    execute(nargs, args);
    fflush(stdin);
    fflush(stdout);
    dup2(ORIG_STDIN, STDIN_FILENO);
    dup2(ORIG_STDOUT, STDOUT_FILENO);
    fflush(stdout);
    fflush(stdin);
  }
  return 0;

}

void hndlsigcc(int signo) {
  fflush(stdin);
  printf("\n%s ", getenv("PROMPT"));
  fflush(stdout);
}

int makeargs(char* input, char* args[]) {
  int i, j, begindex = 0, index = 0, size = 0;
  bool skip = false;
  for(i = 0; args[i] != NULL; i++) {
    free(args[i]);
  }
  for(i = 0; !validate(input[i], END_TOKENS) && i < INPUT_SIZE + 1 && index < ARG_SIZE - 1; i++) {
    if(validate(input[i], SEP_TOKENS) && !skip) {
      input[i] = '\0';
      args[index] = malloc((size + 1) * sizeof(char));
      strcpy(args[index], input + begindex);
      index++;
      size = 0;
      begindex = i + 1;
    }
    else if(validate(input[i], ESC_TOKENS)) {
      skip = !skip;
      for(j = i; !validate(input[j], END_TOKENS) && j < INPUT_SIZE + 1; j++) {
        input[j] = input[j + 1];
      }
      i--;
    }
    else {
      size++;
    }
  }
  args[index < ARG_SIZE ? index : ARG_SIZE - 1] = NULL;
  for(i = 0; args[i] != NULL; i++) {
    if(strcmp(args[i], "") == 0) {
      for(j = i; args[j] != NULL; j++) {
        args[j] = args[j + 1];
      }
      i--;
    }
  }
  for(i = 0; args[i]!= NULL; i++);
  return args[0] == NULL ? 0 : i;
}

bool validate(char c, char* str) {
  int i;
  for(i = 0; str[i] != '\0'; i++) {
    if(str[i] == c) {
      return true;
    }
  }
  return false;
}

int redirect(char* args[]) {
  int j, i;
  for(i = 0; args[i] != NULL; i++) {
    if(strcmp(args[i], ">") == 0) {
      fflush(stdout);
      int file = open(args[i + 1], O_TRUNC | O_WRONLY | O_CREAT);
      dup2(file, STDOUT_FILENO);
      close(file);
      for(j = i; args[j] != NULL; j++) {
        args[j] = args[j + 1];
      }
      for(j = i; args[j] != NULL; j++) {
        args[j] = args[j + 1];
      }
      i--;
    }
    if(strcmp(args[i], "<") == 0) {
      fflush(stdin);
      int file = open(args[i + 1], O_RDONLY);
      if(file == -1) {
        printf("Error! Could not open file.\n");
        return -1;
      }
      else {
        dup2(file, STDIN_FILENO);
        close(file);
        for(j = i; args[j] != NULL; j++) {
          args[j] = args[j + 1];
        }
        for(j = i; args[j] != NULL; j++) {
          args[j] = args[j + 1];
        }
        i--;
      }
    }
  }
  return 0;
}

int pipecmd(char* args[], int fd[]) {
  int i;
  for(i = 0; args[i] != NULL; i++) {
    if(strcmp(args[i], "|") == 0) {
      if(pipe(fd) != 0) {
        return -1;
      }
      else {
        args[i] = NULL;
        return i;
      }
    }
  }
  return 0;
}

void execute(int nargs, char* args[]) {
  int pfd[2];
  if(args[0] == NULL) {
    return;
  }
  else if(strcmp(args[0], "exit") == 0) {
    exit(nargs > 1 ? atoi(args[1]) : 0);
  }
  else if(strcmp(args[0], "cd") == 0) {
    if(nargs < 2) {
      chdir(getenv("HOME"));
      getwd(pwd);
    }
    else {
      if(chdir(args[1]) == 0) {
        getwd(pwd);
      }
      else {
        printf("Error! Could not change directory.\n");
      }
    }
    char* pathvar = malloc(strlen(defpath) + strlen(pwd) + 2);
    strcat(pathvar, pwd);
    strcpy(pathvar, ":");
    strcpy(pathvar, defpath);
    setenv("PATH", pathvar, 1);
    free(pathvar);
  }
  else if(strcmp(args[0], "pwd") == 0) {
    printf("%s\n", pwd);
  }
  else if(strcmp(args[0], "mkdir") == 0) {
    if(nargs > 1) {
      if(mkdir(args[1], 0755) != 0) {
        printf("Error! Could not create directory.\n");
      }
    }
  }
  else if(strcmp(args[0], "rmdir") == 0) {
    if(nargs > 1) {
      if(rmdir(args[1]) == -1) {
        printf("Error! Could not delete directory.\n");
      }
    }
  }
  else if(strcmp(args[0], "show") == 0) {
    printf("%s\n", (nargs > 1 ? getenv(args[1]) : ""));
  }
  else if(strcmp(args[0], "set") == 0) {
    if(nargs > 2) {
      setenv(args[1], args[2], 1);
      printf("Set %s as %s.\n", args[1], args[2]);
    }
  }
  else if(strcmp(args[0], "unset") == 0) {
    if(nargs > 1) {
      unsetenv(args[1]);
      printf("Unset %s.\n", args[1]);
    }
  }
  else if(strlen(args[0]) > 0) {
    int redct = redirect(args);
    int pipe = pipecmd(args, pfd);
    if(redct == -1 || pipe == -1) {
      return;
    }
    pid_t child = fork();
    if(child == -1) {
      printf("Error! Could not create new process.\n");
    }
    else if(child == 0) {
      if(pipe > 0) {
        pid_t fc = fork();
        if(fc == -1) {
          printf("Error! Could not create new process.\n");
        }
        else if(fc == 0) {
          close(pfd[0]);
          fflush(stdout);
          dup2(pfd[1], STDOUT_FILENO);
          close(pfd[1]);
          execvp(args[0], args);
          printf("Error! Could not execute command \"%s\".\n", args[0]);
        }
        else {
          close(pfd[1]);
          fflush(stdin);
          dup2(pfd[0], STDIN_FILENO);
          close(pfd[0]);
          execute(nargs - pipe - 1, args + pipe + 1);
        }
      }
      else {
        execvp(args[0], args);
        printf("Error! Could not execute command \"%s\".\n", args[0]);
      }
      exit(EXIT_FAILURE);
    }
    else {
      if(pipe > 0) {
        close(pfd[0]);
        close(pfd[1]);
      }
      wait(NULL);
    }
  }
}
