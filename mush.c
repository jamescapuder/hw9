#include <fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<ctype.h>
#include<strings.h>
#include<errno.h>
#include<stdlib.h>
#include<string.h>
#include<sys/wait.h>
#include<signal.h>
#include<sys/types.h>

#define LINE_MAX 1024
#define clear() printf("\033[H\033[J")

pid_t child;

void sighandler(int);

void mush_linev_maker(char* line_buffer, char** linev);

void pretty_prompt(){
  char toWd[100];
  getcwd(toWd, sizeof(toWd));
  int len = strlen(toWd)-1;
  int i=0;
  while (toWd[len-i]!='/'){
    i++;
  }
  printf("%.*s @mush ~> ", len, toWd+(len-i));
}

int inputcheck(char** linev){
  int i=0;
  while (linev[i]!=NULL){
    if (!strcmp("<", linev[i])){
      return i;
    }
    i++;
  }
  return -1;
}

int innie(char** linev, int ind){
  if (0>ind){
    return -1;
  }
  int toIn = dup(STDIN_FILENO);
  int read = open(linev[ind+1],O_RDONLY);
  if (0>read){
    perror("input read error\n");
    exit(EXIT_FAILURE);
  }
  if (0>dup2(read,STDIN_FILENO)){
    perror("dup2 input problem \n");
    exit(EXIT_FAILURE);
  }
  linev[ind]=NULL;
  return toIn;
}

int outie(char** linev, int ind){
  if (0>ind){
    return -1;
  }
  int toOut = dup(STDOUT_FILENO);
  int read = open(linev[ind+1],O_WRONLY|O_APPEND|O_CREAT,S_IRUSR|S_IROTH);
  if (0>read){
    perror("output read error\n");
    exit(EXIT_FAILURE);
  }
  if (0>dup2(read,STDOUT_FILENO)){
    perror("dup2 output problem \n");
    exit(EXIT_FAILURE);
  }
  linev[ind]=NULL;
  return toOut;
}
int outputcheck(char** linev){
  int i=0;
  while (linev[i]!=NULL){
    if (!strcmp(">", linev[i])){
      return i;
    }
    i++;
  }
  return -1;
}

void mash_get_line(char* line_buffer){  
  int c;
  int tracker=0;
  while (tracker<LINE_MAX){
    c = getchar();
    if (c=='\n'){
      line_buffer[tracker]='\0';
      break;
    }
    else if (EOF == c){
    exit(1);
    } 
    else{
      line_buffer[tracker]=c;
    }
    tracker++;

  }
}

int is_empty(const char *s) {
  while (*s != '\0') {
    if (!isspace(*s))
      return 0;
    s++;
  }
  return 1;
}

void mush_linev_maker(char* line_buffer, char** linev){
  int tracker=0;
  
  char *cur_tok;
  cur_tok = strtok(line_buffer, " \n");
  while (cur_tok != NULL){
    linev[tracker]=cur_tok;
    printf("token: %s \n", cur_tok);
    tracker++;
    cur_tok = strtok(NULL, " \n");
  }
  
  char* edgearg = linev[--tracker];
  linev[tracker] = is_empty(edgearg) ? NULL : strtok(edgearg, "\n");
}

void sighandler(int signum){
  if (signum==SIGINT){
    if (child>0){
      kill(child, SIGINT);
    }
    signal(SIGINT, sighandler);
  }else{
    signal(signum, sighandler);
  }
  
}

void mush_exec_linev(char** linev){
  pid_t chpid;
  int inno = innie(linev, inputcheck(linev));
  int outno = outie(linev, outputcheck(linev));

  chpid = fork();
  child = chpid;
  if (chpid==0){
    signal(SIGINT, sighandler);
    int stat = execvp(linev[0], linev);
    if (-1==stat){
      perror("command execute error \n" );
      kill(getpid(), SIGKILL);
    }
  } 
  else{
    int wait_log;
    signal(SIGINT, SIG_IGN);
    wait(&wait_log);
    if (inno>=0){
      dup2(inno, 0);
    }
    if (outno>=0){
      dup2(outno, 1);
    }
  
  }
  
}

int mush_parse_linev(char** linev){
  if (linev[0]==NULL){
    
    return 1;
  }
  if (strcmp(linev[0],"cd")==0){
    if (linev[1]==NULL|strcmp(linev[1], "")==0){
      chdir(getenv("HOME"));
    } else{
      int check = chdir(linev[1]);
      if (-1==check){
	perror("DIR NOT FOUND\n");
	exit(EXIT_FAILURE);
      }
    }
  } else if (strcmp(linev[0], "exit")==0){
    exit(0);
  }else if (strcmp(linev[0], "myinfo")==0){
    int procid = getpid();
    int pprocid = getppid();
    printf("Process ID: %d, Parent Process ID: %d\n", procid, pprocid);
  } else {
    mush_exec_linev(linev);
  }
  return 0;
}

void ch_handler(int sig) {
  (void) sig;
  while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {
  }
}

void sig_int_hand(int sig){

  signal(sig, sig_int_hand);
  kill(child, SIGTERM);
  signal(sig, SIG_IGN);

}

int main(){
  char* line_buffer;
  int init_in = dup(0);
  int init_out = dup(1);
  char* line_argv[101]={NULL};
  clear();
  printf("\n==============================\nWelcome to mush, made up shell\n==============================\n");
  signal(SIGINT, SIG_IGN);
  pretty_prompt();
  while (1){
    line_buffer = calloc(LINE_MAX,sizeof(char));
    mash_get_line(line_buffer);
    int tracker=0;
    char *cmd_argv[101];
    int count=1;
    cmd_argv[0]=line_buffer;
    if (strpbrk(line_buffer, "|")!=NULL){
      cmd_argv[tracker]=strtok(line_buffer, "|");
      while (cmd_argv[tracker] != NULL){
	cmd_argv[++tracker]=strtok(NULL, "|");
	count++;
      }
      count--;
    }
    int pipe1[2]={0,0};
    int pipe2[2]={0,0};
    for (int j=0;j<count;j++){
      mush_linev_maker(cmd_argv[j],line_argv );
      if (count>1){
	if (j==0){
	  pipe(pipe1);
	  dup2(pipe1[1],1);
	  close(pipe1[1]);
	}
	if (j+1<count){
	  dup2(pipe1[0],0);
	  close(pipe1[0]);
	  pipe1[0]=pipe1[1]=NULL;
	  
	  pipe1[0]=pipe2[0];
	  pipe1[1]=pipe2[1];
	  dup2(pipe1[1],1);
	  close(pipe1[1]);
	}
	if (j+1==count){
	  dup2(init_out, 1);
	  dup2(pipe1[0], 0);
	  close(pipe1[0]);
	}
      }

      if (line_argv[0]!=NULL){
	mush_parse_linev(line_argv);
      }else{
	pretty_prompt();
	continue;
      }

      signal(SIGCHLD, SIG_IGN);
      signal(SIGCHLD, ch_handler);
    }

    dup2(init_in, 0);
    dup2(init_out, 1);
    init_out = dup(1);
    free(line_buffer);
    pretty_prompt();
  }
  
}
