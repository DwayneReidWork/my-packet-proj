#ifndef SIGNAL_H
#define SIGNAL_H

#include <signal.h>

extern volatile sig_atomic_t child_pid;
extern volatile sig_atomic_t keep_running;

int handle_client(int client_sock);
int signal_handler_setup();
void ctrlc_handler(int dummy);
void timeout_handler(int sig);

#endif