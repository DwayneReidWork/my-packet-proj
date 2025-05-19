#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <sys/types.h>

#define PORT 1234
#define PORT_STR "1234"
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 5

typedef enum __attribute__((__packed__))
{
    PING = 1,
    STRING,
    MUL,
    UPLOAD,
} PacketType;

// Packet Type (Header for All)
typedef struct __attribute__((__packed__))
{
    // msg_tpe is specific type of PacketType
    PacketType msg_type;
} p_header_t;

// String Packets
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    uint32_t data_len;
    uint8_t *data;
} p_string_t;

// Mul Packet Format
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    uint32_t data_len;
    uint32_t *data;
} p_mul_t;

// Upload Packet Format
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    uint32_t data_len;
    uint8_t *data;
} p_upload_t;

int server_setup();
void ctrlc_handler(int dummy);
int handle_upload(int client_sock);
int handle_client(int client_sock);
int handle_ping(int client_sock);
int handle_multiplication(int client_sock);
int check_packet_type(int client_sock);

#endif