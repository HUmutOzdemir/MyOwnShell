#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <regex.h>


#define BUFFER_SIZE 3000
#define READ_END	0
#define WRITE_END	1
#define REGEX_RULE "^\\s*listdir\\s*(-a)?\\s*\\|\\s*grep\\s*\"(.*)\"\\s*"

char *trimwhitespace(char *str){
  char *end;
  // Trim leading space
  while(isspace((unsigned char)*str)) str++;
  if(*str == 0)  // All spaces?
    return str;
  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;
  // Write new null terminator character
  end[1] = '\0';
  return str;
}
// I use regex to find the argument of grep
char *grepRegex(char *all_command){
  size_t maxGroups = 3;
  regex_t regexCompiled;
  regmatch_t groupArray[maxGroups];
  regcomp(&regexCompiled,REGEX_RULE, REG_EXTENDED);
  // If this is one of the grep commands the second group will be the argument of grep
  if (regexec(&regexCompiled, all_command, maxGroups, groupArray, 0) == 0){
   char sourceCopy[strlen(all_command) + 1];
    strcpy(sourceCopy, all_command);
    sourceCopy[groupArray[2].rm_eo] = 0;
    char * temp = sourceCopy + groupArray[2].rm_so;
    regfree(&regexCompiled);
    return temp;
  }
  regfree(&regexCompiled);
  return "";
}

int main(int argc, char *argv[]){
  // History array. I use it as a circular list
  char history[15][BUFFER_SIZE];
  int number_of_commands = 0;
  while(1){
	  pid_t pid;
	  int mypipe1[2];
	  int i;
    // Creates a pipe for whoami child process
	  pipe(mypipe1);
    pid = fork();
	  if (pid == 0) { // whoami-child process
		  printf("whoami child : %d",pid);
      dup2(mypipe1[WRITE_END],STDOUT_FILENO);
      close(mypipe1[WRITE_END]);
		  close(mypipe1[READ_END]);
		  //Execute whoami
      char *const argv[] = {"/usr/bin/whoami", NULL};
      execv(argv[0], argv);
      //If a problem occurs in exec
      exit(1);
	  }
    close(mypipe1[WRITE_END]);
    wait(NULL);
    char username[BUFFER_SIZE];
    //When whoami child terminates reads the user name from pipe and close the pipe
    read(mypipe1[READ_END],username,BUFFER_SIZE);
    close(mypipe1[READ_END]);
    printf("%s >>> ",trimwhitespace(username));
    char all_command[BUFFER_SIZE];
    //Reads the current command and puts it in the history array(modulo operator because of circular list)
    fgets(all_command,BUFFER_SIZE,stdin);
    strcpy(history[number_of_commands % 15],all_command);
    //Finds the argument of grep if there is grep, otherwise empty string
    char *grepArgument = grepRegex(all_command);
    number_of_commands += 1;
    char delim[] = " \n";
    //Splits the command from whitespaces
    char *command = strtok(all_command, delim);
    // if command is exit exits the program
    if(strcmp(command,"exit")==0){
      exit(0);
    }
    pid = fork();
    if(pid == 0){ //Creates child process to execute command
      char *current_command = command;
      command = strtok(NULL,delim);
      if(strcmp(current_command,"listdir")==0){ // Checks the command is listdir
          int flag = 0;
          if(command!=NULL){
              if(strcmp(command,"-a")==0){ // Check there is -a flag or not
                flag = 1;
                command = strtok(NULL,delim);
              }
              if(command !=NULL && strcmp(command,"|")==0){// Check there is a pipe operator
                command = strtok(NULL,delim);
                // Creates the pipe
                pipe(mypipe1);
                pid = fork();
                if(pid == 0){// Grant child of the main process. (It executes the ls command)
                  dup2(mypipe1[WRITE_END],STDOUT_FILENO); // Makes the stdout write end of the pipe
                  close(mypipe1[WRITE_END]);//Closes both ends of pipe
                  close(mypipe1[READ_END]);
                  if(flag){ // Executes the ls command
                    char *const argv[] = {"/bin/ls","-a", NULL};
                    execv(argv[0], argv);
                  }else{
                    char *const argv[] = {"/bin/ls", NULL};
                    execv(argv[0], argv);
                  } 
                }else{// Child process of the main process
                  dup2(mypipe1[READ_END],STDIN_FILENO); //Makes the stdin read end of the pipe
                  close(mypipe1[READ_END]);//Closes both ends of pipe
                  close(mypipe1[WRITE_END]);
                  execl("/bin/grep", "grep", grepArgument , NULL);// Executes the grep command(argument comes from the initial part)
                }
              }else{
                char *const argv[] = {"/bin/ls","-a", NULL}; // Execute ls -a command without pipe
                execv(argv[0], argv);
              }
          }else{
            char *const argv[] = {"/bin/ls", NULL};// Execute ls command without pipe
            execv(argv[0], argv);
          }
      }else if(strcmp(current_command,"currentpath")==0){ // Execute pwd command
        char *const argv[] = {"/bin/pwd", NULL};
        execv(argv[0], argv);
      }else if(strcmp(current_command,"printfile")==0){ // Execute cat command
        char * input_file = command;
        command = strtok(NULL,delim);
        if(command != NULL && strcmp(command,">")==0){ // If there is a redirect operator
          command = strtok(NULL,delim);
          fclose(fopen(command,"w"));
          int fd = open(command,O_WRONLY | O_CREAT,0777);// Create the file descriptor
          dup2(fd,STDOUT_FILENO); // Make stdout the argument file
        }
        execl("/bin/cat", "cat", input_file , NULL); // Execute cat
      }else if(strcmp(current_command,"footprint")==0){ // Prints the last commands from the history array
        if(number_of_commands<=15){
          for(i=0;i<number_of_commands;i++){
            printf("%d %s",i+1,history[i]);
          }
        }else{
          int start_index = number_of_commands % 15;
          int last_index = start_index -1;
          int num = number_of_commands - 14;
          while((start_index % 15)!=last_index){
            printf("%d %s",num,history[start_index % 15]);
            start_index++;
            num++;
          }
          printf("%d %s",num,history[start_index % 15]);
        }
      }else{
        printf("Command Not Found\n");
      }
      return 0;
    }else{ //Parent Process waits for the current command
      wait(NULL);
    }

  }
	return 0;
 
}