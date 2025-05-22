#include <arpa/inet.h>
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
#define BUFFER 1024
#define BASE_PATH "server/for_uploads/"

void ctrlc_handler(int dummy);
int handle_upload(int client_sock, p_upload_t *upload_pack);
int handle_client(int client_sock);
int handle_ping(int client_sock);
int handle_multiplication(int client_sock, p_mul_t *mulpack);
int receive_packet(int client_sock, p_header_t *header, uint8_t **dest);
volatile sig_atomic_t keep_running = 1;

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

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
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

    if (handle_client(new_socket) < 0)
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

    while (keep_running)
    {
        switch (receive_packet(client_sock, &header, &data))
        {
        case PING:
            printf("Ping Recieved\n");
            handle_ping(client_sock);
            break;
        case STRING:
            break;
        case MUL:
            printf("Multiplication Request Recieved\n");
            p_mul_t mul_pack;
            if (htonl(header.p_len) != sizeof(mul_pack.num))
            {
                perror("Size mismatch between header and mulpack.num");
                free(data);
                return -1;
            }
            memcpy(&mul_pack.header, &header, sizeof(header));
            memcpy(&mul_pack.num, data, sizeof(mul_pack.num));

            if (0 != handle_multiplication(client_sock, &mul_pack))
            {
                perror("Eror in Handle Multiplication");
                free(data);
                return -1;
            }
            break;
        case UPLOAD:
            p_upload_t upload_pack = {.upload_data = data};
            memcpy(&upload_pack.header, &header, sizeof(header));
            printf("Upload Packet Received.\n");

            if (0 != handle_upload(client_sock, &upload_pack))
            {
                perror("Error in Handle Upload");
                free(data);
                return -1;
            }
            break;
        default:
            printf("Invalid Packet Type Received\n");
            break;
        }
        if (NULL != data)
        {
            free(data);
        }
    }
    return 0;
}

int receive_packet(int client_sock, p_header_t *header, uint8_t **dest)
{
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

    if (0 > bytes)
    {
        perror("Recieve Failed\n");
        return -1;
    }
    // ctrl+d handler?
    if (0 == bytes)
    {
        printf("Client disconnected.\n");
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
    printf("Inside Ping\n");

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
    // Convert number endianness multiply by 2 and reassign to mul.num

    uint32_t result = htonl(ntohl(mulpack->num) * 2);

    p_mul_t mul_response = {.header.msg_type = MUL, .num = result, .header.p_len = sizeof(mulpack->num)};

    ssize_t bytes_sent = send_all(client_sock, (const uint8_t *)&mul_response, sizeof(p_mul_t));
    if (0 > bytes_sent)
    {
        perror("Send failed");
        return -1;
    }
    return 0;
}

int handle_upload(int client_sock, p_upload_t *upload_pack)
{
    printf("Inside handle upload function\n");

    // Find the delimiter '|' to separate filename and content
    char *filename = (char *)upload_pack->upload_data;
    char *delimiter_pos = strchr(filename, '|');

    if (NULL == delimiter_pos)
    {
        fprintf(stderr, "Delimiter not found in upload data.\n");
        return -1;
    }

    // Split into filename and content based on the delimiter
    *delimiter_pos = '\0';             // Null-terminate the filename
    char *content = delimiter_pos + 1; // Content starts after the delimiter
    ssize_t content_size = upload_pack->header.p_len - (content - filename);
    if (0 == content_size)
    {
        perror("No content found.");
        return -1;
    }
    if (0 == strlen(filename))
    {
        perror("Filename Not Found.");
        return -1;
    }

    // Construct the full file path
    int fd;

    char full_path[BUFFER];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_PATH, filename);

    // Open file for writing (create file if it doesn't exist)
    fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open with write-only, create, and truncate flags
    if (-1 == fd)
    {
        perror("Error opening file");
        // Consider adding an error packet to send back in case of errors.
        return -1;
    }

    // Write contents to the file using write system call
    ssize_t bytes_written = 0;
    while (bytes_written < content_size)
    {
        bytes_written = write(fd, content, content_size);
        if (-1 == bytes_written)
        {
            perror("Error writing to file");
            close(fd);
            return -1;
        }
    }

    // Close the file descriptor after writing
    if (-1 == close(fd))
    {
        perror("Error closing file");
        return -1;
    }

    printf("Write Successful\n");

    // Send reply
    uint8_t *success_msg = (uint8_t *)"Upload Successful";
    p_header_t upload_reply = {.msg_type = UPLOAD, .p_len = strlen((const char *)success_msg) + 1};

    ssize_t bytes_sent = send_all(client_sock, success_msg, sizeof(upload_reply));
    if (0 > bytes_sent)
    {
        perror("Send failed");
        return -1;
    }

    ssize_t upload_reply_sent = send_all(client_sock, success_msg, upload_reply.p_len);
    if (0 > upload_reply_sent)
    {
        perror("Send failed");
        return -1;
    }

    printf("Upload Response Sent\n");
    return 0;
}

void ctrlc_handler(int dummy)
{
    keep_running = 0;
}
