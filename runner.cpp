#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNNER_UID 1005
#define RUNNER_GID 1005

void print_preformance(const struct rusage * ruse) {
    unsigned time = ruse->ru_utime.tv_sec * 1000 + ruse->ru_utime.tv_usec / 1000;
    time += ruse->ru_stime.tv_sec * 1000 + ruse->ru_stime.tv_usec / 1000;
    fprintf(stderr, "Total time: %u seconds and %u milliseconds. Memory: %ld\n", time / 1000 , time % 1000, ruse->ru_maxrss);
    printf("TIME_MILLISECONDS=%u\n", time);
    printf("MEMORY=%ld\n", ruse->ru_maxrss);

}

int main(int argc, char ** argv)
{
    if (argc <= 1)
        return 1;
    pid_t pid = fork();
    if (pid) {
        // monitor
        int status;
        struct rusage ruse;
        wait4(pid, &status, 0, &ruse);
        if (WIFEXITED(status)) {
            // exited normally
            int exit_code = WEXITSTATUS(status);
            fprintf(stderr, "Exit code: %d\n", exit_code);
            printf("EXIT_CODE=%d\n", exit_code);
        } else if (WIFSIGNALED(status)) {
            // killed by signal
            int signal = WTERMSIG(status);
            fprintf(stderr, "Signal: %s\n", strsignal(signal));
            printf("SIGNAL=%d\n", signal);
        } else {
            // should never get there
            fprintf(stderr, "Unknown status: %d\n", status);
            printf("UNKNOWN=%d\n", status);
        }
        print_preformance(&ruse);
    } else {
        // runner    
        char ** args = (char **) malloc(argc * sizeof(char *));
        int i = 0;
        for (i = 1 ; i < argc ; ++i)
            args[i - 1] = argv[i];
        args[i - 1] = NULL;
        if (!freopen("/tmp/stdin.txt", "r" , stdin))
            perror("freopen");
        if (!freopen("/tmp/stdout.txt", "w", stdout))
            perror("freopen");
        if (!freopen("/tmp/stderr.txt", "a+", stderr))
            perror("freopen");
        if (fchmod(STDIN_FILENO, 0600) 
            || fchmod(STDOUT_FILENO, 0600) 
            || fchmod(STDERR_FILENO, 0600))
            perror("fchmod");
        // chroot if using static runner
        #ifdef RUNNER_STATIC
        if (chroot("/target"))
            perror("chroot");
        #endif
        // optionally set uid & gid
        if (setgid(RUNNER_GID))
            perror("setgid");
        if (setuid(RUNNER_UID))
            perror("setuid");
        // check for TIME_LIMIT env
        char * time_limit_str = getenv("TIME_LIMIT");
        if (time_limit_str)
        {
            unsigned long long ul = 0;
            sscanf(time_limit_str, "%llu", &ul);
            struct rlimit rc;
            rc.rlim_cur = ul;
            rc.rlim_max = ul + 1;
            setrlimit(RLIMIT_CPU, &rc);
            unsetenv("TIME_LIMIT");
        }        
        if (execvp(argv[1] , args))
        {
            perror("exec");
            return 2;
        }
    }
    return 0;
}

