#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

void run(char *argv[]);
int child(void *argv);
void cgroup(pid_t pid);

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Usage: %s run <command> [args...]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "run") == 0) {
        run(&argv[2]);
    } else {
        printf("Command not recognized\n");
        return 1;
    }

    return 0;
}

void run(char *argv[]) {

    if(argv[0] == NULL) {
        printf("No args provided\n");
        return;
    }

    char *stack = malloc(1024 * 1024);
    if (!stack) {
        perror("stack malloc failed");
        exit(1);
    }
    const int flags = CLONE_NEWPID // new pid namespace with pid 1
     | CLONE_NEWUTS // new uts namespace for hostname
      | CLONE_NEWNS // new mount namespace without affecting host mounts
       | CLONE_NEWUSER // new user namespace
        | SIGCHLD; // signal sent when child terminates to parent

    pid_t pid = clone(child, stack + 1024 * 1024, flags, argv); 
        // we user fork to create an isolated process

    if (pid == -1) {
        perror("clone failed");
        free(stack);
        return;
    }

    cgroup(pid);

    waitpid(pid, NULL, 0);
    free(stack);
}

int child(void *arg) {
    char **argv = arg;
    printf("Child running %s as pid %d\n", argv[0], getpid());

    if(sethostname("container", 9) == -1) {
        perror("sethostname failed");
        return 1;
    }

    if(chroot("/home/ocker/root") == -1) {
        perror("chroot failed");
        return 1;
    }
    if(chdir("/") == -1) {
        perror("chdir failed");
        return 1;
    }

    if(mount("proc", "/proc", "proc", 0, "") == -1) {
        perror("mount /proc failed");
        return 1;
    }
    // mount proc filesystem so container can excute commands like ps
    // proc inside container will show only processes inside the container

    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);
    // to prevent container mounts from affecting host

    if(execvp(argv[0], argv) == -1) {
        perror("execvp failed");
    }
    return 1;
}

int wfile(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if(!f) {
        perror(path);
        return -1;
    }
    if(fprintf(f, "%s", value) < 0) {
        perror(path);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

void cgroup(pid_t pid) {
    char path[256], buf[32];

    const char *cgroup_path = "/sys/fs/cgroup/mycontainer";
    snprintf(path, sizeof(path), "%s", cgroup_path);

    if(mkdir(path, 0755) == -1) {
        perror("mkdir cgroup failed");
        exit(1);
    } // create cgroup directory if not exists
                       // inside /sys/fs/cgroup

                       // set memory limit to 50MB
    snprintf(path, sizeof(path), "%s/memory.max", cgroup_path);
    snprintf(buf, sizeof(buf), "%d", 50*1024*1024);
    wfile(path, buf);

                      // set pids limit to 20
    snprintf(path, sizeof(path), "%s/pids.max", cgroup_path );
    wfile(path, "20");

                     // add the process to this cgroup
    snprintf(path, sizeof(path), "%s/cgroup.procs", cgroup_path);
    snprintf(buf, sizeof(buf), "%d", pid);
    wfile(path, buf);

    setpriority(PRIO_PROCESS, pid, 10); 
    // adjust the priority of the process with (nice value) which affects SCHED_OTHER scheduling and cput time it gets
    // we lowered the priority to 10 wich you can tell it corresponds to the queue level 110 
}
