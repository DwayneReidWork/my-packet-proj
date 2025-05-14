#ifndef NETCOMM_H
#define NETCOMM_H

#include <stdlib.h>

char *recv_expected(int sock, size_t expected_len);
int send_all(int sock, const char *data, size_t data_len);

#endif