#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include "../inc/netcomm.h"
#include "../inc/server.h"

#define PORT 1234
#define BASE_PATH "server/for_uploads/"

int handle_client(int client_sock);
int handle_list_files(int client_sock);
int handle_ping(int client_sock);
int handle_multiplication(int client_sock, p_mul_t *mulpack);
int handle_upload(int client_sock, p_upload_t *upload_pack);
int receive_packet(int client_sock, p_header_t *header, uint8_t **dest);
void ctrlc_handler(int dummy);
volatile sig_atomic_t keep_running = 0;

int server_setup()
{
    struct sigaction sa;
    sa.sa_handler = ctrlc_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int status = 0;
    int addrlen = sizeof(address);

    if (-1 == (server_fd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("Socket Failure");
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (0 > bind(server_fd, (struct sockaddr *)&address, sizeof(address)))
    {
        perror("bind failed");
        close(server_fd);
        return -1;
    }

    if (0 > listen(server_fd, 3))
    {
        perror("listen failed");
        close(server_fd);
        return -1;
    }

    printf("Listening on port %d\n", PORT);

    if (0 > (new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)))
    {
        perror("accept failed");
        close(server_fd);
        return -1;
    }
    char host_buff[NI_MAXHOST];
    char port_buff[NI_MAXSERV];

    if (0 != (status = getnameinfo((struct sockaddr *)&(address), addrlen, host_buff, NI_MAXHOST, port_buff, NI_MAXSERV, NI_NUMERICSERV)))
    {
        fprintf(stderr, "Failed to get name information for client (%d: %s)", status, gai_strerror(status));
        return -1;
    }

    printf("Connection Recieved from %s:%s\n", host_buff, port_buff);

    if (0 > handle_client(new_socket))
    {
        perror("Error Handling Client");
    }

    printf("\nShutting down server...\n");
    close(new_socket);
    close(server_fd);

    return 0;
}

int handle_client(int client_sock)
{
    uint8_t *data = NULL;
    p_header_t header;

    while (-1 != keep_running)
    {
        switch (receive_packet(client_sock, &header, &data))
        {
        case PING:
            handle_ping(client_sock);
            break;
        case STRING:
            break;
        case MUL:
            p_mul_t mul_pack;

            if (ntohl(header.p_len) != sizeof(mul_pack.num))
            {
                perror("Size mismatch between header and mulpack.num");
                free(data);
                return -1;
            }

            memcpy(&mul_pack.header, &header, sizeof(header));
            memcpy(&mul_pack.num, data, sizeof(mul_pack.num));

            if (0 != handle_multiplication(client_sock, &mul_pack))
            {
                perror("Error in Handle Multiplication");
                free(data);
                return -1;
            }
            break;
        case UPLOAD:
            // Allocate a dynamic buffer to hold header + data
            uint32_t payload_len = ntohl(header.p_len);
            p_upload_t *upload_pack = malloc(sizeof(p_upload_t) + payload_len);
            if (!upload_pack)
            {
                perror("malloc failed");
                free(data);
                return -1;
            }

            memcpy(&upload_pack->header, &header, sizeof(header));
            memcpy(upload_pack->upload_data, data, payload_len);

            if (0 != handle_upload(client_sock, upload_pack))
            {
                perror("Error in Handle Upload");
                free(data);
                free(upload_pack);
                return -1;
            }

            free(upload_pack);
            break;
        case LIST_FILES:
            printf("Received List File Packet.\n");
            handle_list_files(client_sock);
            break;
        case RUN_CMD:
            printf("Received Cmd to run.\n");
            break;
        default:
            printf("Invalid Packet Type Received\n");
            break;
        }
    }
    return 0;
}

int receive_packet(int client_sock, p_header_t *header, uint8_t **dest)
{
    printf("Waiting for Packet.\n");
    if (NULL == header)
    {
        perror("Header Invalid");
        return -1;
    }

    if (NULL == dest)
    {
        perror("Dest Invalid");
        return -1;
    }

    ssize_t bytes = recv_expected(client_sock, sizeof(p_header_t), (uint8_t *)header, sizeof(p_header_t));

    printf("Packet Received.\n");
    if (0 > bytes)
    {
        perror("Receive Failed\n");
        return -1;
    }

    printf("Recvd Packet Type: %d\n", header->msg_type);
    printf("Recvd Packet Len: %d\n", htonl(header->p_len));

    *dest = recv_expected_alloc(client_sock, htonl(header->p_len));

    if (NULL == *dest)
    {
        perror("Malloc Failed");
        return -1;
    }

    return header->msg_type;
}

int handle_ping(int client_sock)
{
    // create packet
    p_header_t ping_reply = {.msg_type = PING, .p_len = 0};

    ssize_t bytes_sent = send_all(client_sock, (const uint8_t *)&ping_reply, sizeof(ping_reply));
    if (0 > bytes_sent)
    {
        perror("Send failed");
        return -1;
    }

    printf("PING Response Sent\n");
    return 0;
}

int handle_multiplication(int client_sock, p_mul_t *mulpack)
{
    int32_t num_host = ntohl(mulpack->num);

    int32_t result = num_host * 2;

    p_mul_t mul_response = {.header.msg_type = MUL,
                            .header.p_len = htonl(sizeof(mulpack->num)),
                            .num = htonl(result)};

    ssize_t bytes_sent = send_all(client_sock, (const uint8_t *)&mul_response, sizeof(p_mul_t));
    if (0 > bytes_sent)
    {
        perror("Send failed");
        return -1;
    }
    printf("Multiplication Response Sent.\n");
    return 0;
}

int handle_upload(int client_sock, p_upload_t *upload_pack)
{
    p_header_t header = upload_pack->header;
    uint32_t payload_len = ntohl(header.p_len);

    p_upload_t *upload_pkt = malloc(sizeof(p_upload_t) + payload_len);
    if (NULL == upload_pkt)
    {
        perror("Failed to allocate upload packet");
        return -1;
    }

    // fixed warning of uninitialized thingy
    memset(upload_pkt, 0, sizeof(p_upload_t) + payload_len);

    memcpy(&upload_pkt->header, &upload_pack->header, sizeof(header));
    memcpy(upload_pkt->upload_data, upload_pack->upload_data, payload_len);

    // Parse upload_data: "filename|filecontents"
    char *filename = (char *)upload_pkt->upload_data;
    char *delimiter_pos = strchr(filename, '|');

    if (NULL == delimiter_pos)
    {
        fprintf(stderr, "Delimiter not found in upload data.\n");
        free(upload_pkt);
        return -1;
    }

    *delimiter_pos = '\0';
    char *content = delimiter_pos + 1;

    size_t filename_len = strlen(filename) + 1;
    size_t content_size = payload_len - filename_len;

    if (0 == strlen(filename) || 0 == content_size)
    {
        fprintf(stderr, "Filename or content missing\n");
        free(upload_pkt);
        return -1;
    }

    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_PATH, filename);

    int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (-1 == fd)
    {
        perror("Error opening file");
        free(upload_pkt);
        return -1;
    }

    ssize_t bytes_written = 0;
    while (bytes_written < content_size)
    {
        ssize_t written = write(fd, content + bytes_written, content_size - bytes_written);
        if (0 >= written)
        {
            perror("Error writing to file");
            close(fd);
            free(upload_pkt);
            return -1;
        }
        bytes_written += written;
    }

    close(fd);
    free(upload_pkt);
    printf("Write Successful\n");

    const char *msg = "Upload Successful";
    size_t msg_len = strlen(msg) + 1;

    p_upload_t *upload_reply = malloc(sizeof(p_upload_t) + msg_len);
    if (NULL == upload_reply)
    {
        perror("Failed to allocate upload reply");
        return -1;
    }

    upload_reply->header.msg_type = UPLOAD;
    upload_reply->header.p_len = htonl(msg_len);
    memcpy(upload_reply->upload_data, msg, msg_len);

    ssize_t bytes_sent = send_all(client_sock, (uint8_t *)upload_reply, sizeof(p_header_t) + msg_len);
    if (0 > bytes_sent)
    {
        perror("Send Failed");
        free(upload_reply);
        return -1;
    }

    free(upload_reply);
    printf("Upload response sent successfully.\n");

    return 0;
}

int handle_list_files(int client_sock)
{
    p_list_files_t *reply_file_list;
    reply_file_list = malloc(sizeof(p_list_files_t));

    if (NULL == reply_file_list)
    {
        perror("malloc failed");
        return -1;
    }
    memset(reply_file_list, 0, sizeof(p_list_files_t));

    struct dirent *ent;
    reply_file_list->file_name_count = 0;
    DIR *dir = opendir(BASE_PATH);

    if (NULL == dir)
    {
        perror("Error opening directory");
        free(reply_file_list);
        return -1;
    }

    while (NULL != (ent = readdir(dir)))
    {
        if (ent->d_type == DT_REG)
        {
            // realloc makes space for the new pointer
            reply_file_list->dirFileName = realloc(reply_file_list->dirFileName, reply_file_list->file_name_count * sizeof(char *) + 1);

            // copy name into struct
            char *temp = strdup(ent->d_name);

            reply_file_list->dirFileName[reply_file_list->file_name_count] = strdup(ent->d_name);
            // get name length assign to struct
            reply_file_list->dirFileNameLen[reply_file_list->file_name_count] = strlen(temp);

            // increment file name counter
            reply_file_list->file_name_count++;
        }
    }
    closedir(dir);

    reply_file_list->header.msg_type = LIST_FILES;
    // p_len = size of file names + number of file names
    reply_file_list->header.p_len = htons(
        sizeof(reply_file_list->dirFileName) +
        sizeof(reply_file_list->dirFileNameLen));

    printf("List Reply Header P_Len: %d\n", reply_file_list->header.p_len);

    p_list_files_t *resized_reply_file_list;
    resized_reply_file_list = realloc(reply_file_list, reply_file_list->header.p_len);

    reply_file_list = resized_reply_file_list;

    ssize_t total_size = sizeof(resized_reply_file_list) + htons(reply_file_list->header.p_len);
    // ssize_t total_size = sizeof(p_header_t) + htonl(reply_file_list->header.p_len);
    send_all(client_sock, (uint8_t *)reply_file_list, total_size);
    free(reply_file_list);
    printf("List Sent!\n");

    return 0;
}

void ctrlc_handler(int dummy)
{
    const char *error_message = "\nCTRL+C signal received.\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    keep_running = -1;
}