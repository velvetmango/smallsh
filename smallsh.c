//Marcela Echavarria

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<signal.h>
#include <errno.h>
#include <pwd.h>


//global variables
char line [BUFSIZ];
char * tokensIn[512];
char * tokensOut[512];
int shellpid;
char shellpidString [64];
pid_t child;
char * temp;
char BUFFER[64];
struct sigaction sigintAction;
struct sigaction sigtstpAction;
int fgOnly = 0;

//signal handler
void handle_sigint (int sig)
{
    return;
}
//signal handler
void handle_sigtstp (int sig)
{
    if (fgOnly){
        fprintf(stderr,"Exiting foreground-only mode\n");
        fgOnly = 0;
    }
    else {
        fprintf(stderr, "Entering foreground-only mode (& is now ignored)\n");
        fgOnly = 1;
    }
    return;
}


int main (){

    int status = 0;
    int bg; //foreground
    char * token;
    int i;
    int j;
    pid_t backgroundPid = 0;
    int lastToken;
    int newInput = -1;
    int newOutput = -1;
    pid_t waitingForPid = 0;


    //setting up signal handlers
    sigintAction.sa_handler = handle_sigint;
    sigintAction.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigintAction, NULL);

    sigtstpAction.sa_handler = handle_sigtstp;
    sigtstpAction.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sigtstpAction, NULL);

    do {

      next:
        bg = 0; //bg true
        newInput = -1;
        newOutput = -1;

        //clearing token array
        for(i = 0; i < 512; i++) {
            tokensIn[i] = NULL;
            tokensOut[i] = NULL;
        }
        //replacing new line with null
        line[0] = '\0';

        //getting status of children
        if(((backgroundPid = waitpid(-1, &status, WNOHANG)) > 0)){
            if(WIFSIGNALED(status)){
                printf("Terminated by signal %d\n", WTERMSIG(status));
                fflush(stdout);
            }
            else if (WIFEXITED(status)){
                printf("exit value %d\n", status);
                fflush(stdout);
            }

            if (waitingForPid == backgroundPid){
                //waitingForPid = 0;
                if(waitpid(waitingForPid,&status,WNOHANG)>0) {
                    printf("exit value %d\n", status);
                    fflush(stdout);
                }

            }
        }
        printf(": "); //prompt
        fflush(stdout);


        if(fgets(line, BUFSIZ,stdin) == NULL) {
            perror("smallsh fgets"); //error
            return 1; //status
        }
        line[strlen(line)-1]='\0'; //line is terminated with null

        //blank line
        if ((tokensIn[0] = strtok(line, " ")) == NULL){
            continue;
        }

        //tokenizing and copying tokens into tokensIN array
        for(i=1; (token = strtok(NULL, " "))!= NULL; i++){
            tokensIn[i] = token;
        }
        tokensIn[i] = NULL;
        lastToken = i-1;
        i=0;
        //checking for &
        if (strcmp(tokensIn[lastToken], "&") == 0){
            bg = 1;
            tokensIn[lastToken] = NULL;
        }
        tokensOut[0] = NULL;

        //moving tokens from tokensIN to tokensOut
        for(i=0, j=0; tokensIn[i] != NULL; i++) {

            //redirection
            if (strcmp(tokensIn[i], "<") == 0) {
                i++; //next token
                if ((newInput = open(tokensIn[i],O_RDONLY)) == -1) {
                    perror("badfile");
                    goto next;
                }
                continue;
            }
            //redirection
            else if (strcmp(tokensIn[i], ">") == 0) {
                i++; //next token
                if ((newOutput = open(tokensIn[i],
                                      (O_WRONLY | O_CREAT),0777)) == -1) {
                    perror("badfile");
                    goto next;
                }
                continue; //next token
            }
            //comments
            else if (strstr(tokensIn[i],"#") != 0) {
                goto next; //go back to prompt
            }
            //replace $$ with pid
            else if ((temp = strstr(tokensIn[i], "$$")) != NULL) { //find $$ within token
                shellpid = getpid(); //getpid
                strcpy(BUFFER,tokensIn[i]);

                temp = strstr(BUFFER, "$$");
                *temp = '\0';
                sprintf(shellpidString, "%s%d", BUFFER, shellpid); //formatting string into buffer
                tokensIn[i] = shellpidString; //token replaced by shellpidString number
            }
            //added to tokensOut if passing through exec
            tokensOut[j++]= tokensIn[i];
        } //end of for loop

        //exit built in
        if (strcmp(tokensOut[0],"exit") == 0) {
            fflush(stdin);
            exit (0);

        }
        //status built in
        else if (strcmp(tokensOut[0],"status") == 0) {
            if(WIFSIGNALED(status)){
                printf("Terminated by signal %d\n", WTERMSIG(status));
                fflush(stdout);
            }
            else if (WIFEXITED(status)){
                if (status != 0) {
                    status = 1;
                }
                printf("exit value %d\n", status);
                fflush(stdout);
            }

        }
        //cd built in
        else if (strcmp(tokensOut[0], "cd") == 0) {
            if (tokensOut[1]== NULL) {
                struct passwd * pw = getpwuid(getuid());
                chdir(pw -> pw_dir); //set to home directory
            }
            else{
                chdir(tokensOut[1]);
            }
        }

        //begin fork
        else {
            child = fork();
            switch (child) {
              case -1:
                fprintf(stderr, "fork failed\n");
                return 1;

                //child case
              case 0:
                if (newInput >= 0) {
                    dup2(newInput,0); //0=stdin
                }
                if (newOutput >= 0) {
                    dup2(newOutput,1); //1=stdout
                }
                execvp(tokensOut[0], tokensOut);
                perror("badfile");

              default:
                status = 0;
                int w;
                if ((bg == 0) || (fgOnly == 1)) {
                    do {
                        w = wait(&status);
                        if( w == -1){
                            if(errno == EINTR){
                                continue;
                            } else if (errno == ECHILD) {
                                break ;
                            } else {
                                perror("waitpid error");
                                break ;
                            }
                        }
                    } while(w <= 0);

                    waitingForPid = 0;
                } else {
                    waitingForPid = child;
                    printf("%d\n", child);
                }
            }
        }

    } while (!feof(stdin));


    return 0;
}
