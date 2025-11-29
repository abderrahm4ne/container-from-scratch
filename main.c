// container from scratch using C
// main.c

#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>


#define STACK_SIZE 1024*1024 

void run(char *argv[]);
void child(char *argv[]);

int main(int argc, char *argv[]) {

    if(argc < 2) {
        printf("Usage main.c but no command line line provided \n");
        return 1;
    }

    if (strcmp(argv[1], "run") == 0) {
        run(&argv[2]);
    }
    else if(strcmp(argv[1], "child") == 0) {
        child(&argv[2]);
    } else {
        printf("Command not recognized\n");
    }

        return 0;
}


void run(char *argv[]) {
    if(argv[0] == NULL) {
        printf("No command provided\n");
        return;
    }

    char *stack = malloc(STACK_SIZE);
    if(!stack) { perror("malloc failed"); return; }

    pid_t pid = clone(child, stack + STACK_SIZE, CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, argv);

    if(pid == -1) { perror("clone failed"); free(stack); return; }

    waitpid(pid, NULL, 0);
    free(stack);
}

void child(char *argv[]) {
    char **cmd = argv;

    printf("Child running command:\n");
    for(int i=0; cmd[i] != NULL; i++)
        printf("  Arg %d: %s\n", i, cmd[i]);

    execvp(cmd[0], cmd);
    perror("execvp failed");
    return 1; 
}