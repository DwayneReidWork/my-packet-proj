#ifndef PACKET_H
#define PACKET_H

#include <stdlib.h>
#include <stdint.h>

#include "../inc/server.h"

int handle_list_files(int client_sock);
int handle_ping(int client_sock);
int handle_multiplication(int client_sock, p_mul_t *mulpack);
int handle_run_cmd(int client_sock, p_cmd_t *cmd_pack, char *cmd);
int handle_upload(int client_sock, p_upload_t *upload_pack);
int receive_packet(int client_sock, p_header_t *header, uint8_t **dest);
int serializeListPacket(p_list_files_t *pckt, uint8_t **buff);

#endif