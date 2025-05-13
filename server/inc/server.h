#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <sys/types.h>

#define PORT 1234
#define PORT_STR "1234"
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 5

void server_setup();

#endif