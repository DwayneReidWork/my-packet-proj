#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../inc/signal.h"

volatile sig_atomic_t child_pid = -1;
volatile sig_atomic_t keep_running = 0;

void ctrlc_handler(int dummy)
{
    const char *error_message = "\nCTRL+C signal received.\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    keep_running = -1;
}

void timeout_handler(int sig)
{
    if (child_pid > 0)
    {
        kill(child_pid, SIGKILL);
    }
}

int signal_handler_setup()
{
    struct sigaction sa;
    sa.sa_handler = ctrlc_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
        return -1;
    }

    struct sigaction sa_alrm;
    sa_alrm.sa_handler = timeout_handler;
    sa_alrm.sa_flags = 0;
    sigemptyset(&sa_alrm.sa_mask);
    sigaction(SIGALRM, &sa_alrm, NULL);
    if (sigaction(SIGALRM, &sa_alrm, NULL) == -1)
    {
        perror("sigaction SIGALRM");
        return -1;
    }
    return 0;
}