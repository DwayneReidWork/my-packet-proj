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
    LIST_FILES,
    RUN_CMD,
} PacketType;

// Packet Type (Header for All)
typedef struct __attribute__((__packed__))
{
    // msg_tpe is specific type of PacketType
    PacketType msg_type;
    uint32_t p_len;
} p_header_t;

// Mul Packet Format
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    uint32_t num;
} p_mul_t;

// Upload Packet Format
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    uint8_t upload_data[];
} p_upload_t;

// List File Packet Format (Should just use ping format)
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    uint8_t file_count;
    uint8_t *FileNameLen;
    char **FileName;
} p_list_files_t;

// Run Cmd Packet Format
typedef struct __attribute__((__packed__))
{
    p_header_t header;
    char **cmd_output;
} p_cmd_t;

int server_setup();

#endif