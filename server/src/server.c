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

    while (keep_running)
    {
        if (handle_client(new_socket) < 0)
        {
            break;
        }
    }
    printf("\nShutting down server...\n");
    close(new_socket);
    close(server_fd);

    return 0;
}

int handle_client(int client_sock)
{
    switch (check_packet_type(client_sock))
    {
    case PING:
        printf("Ping Recieved\n");
        handle_ping(client_sock);
        break;
    case STRING:
        break;
    case MUL:
        printf("Multiplication Packet Received.\n");
        handle_multiplication(client_sock);
        break;
    case UPLOAD:
        printf("Upload Packet Received.\n");
        handle_upload(client_sock);
        break;
    default:
        printf("Invalid Packet Type Received\n");
        break;
    }
    return 0;
}

int check_packet_type(int client_sock)
{
    PacketType packet_type;

    ssize_t bytes = recv(client_sock, &packet_type, sizeof(PacketType), 0);
    printf("Recvd Packet Type Raw: %d\n", packet_type);

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

    return packet_type;
}

int handle_ping(int client_sock)
{
    const char *reply_msg = "Got your Ping!";
    size_t msg_len = strlen(reply_msg) + 1;

    // create packet
    p_string_t ping_reply;
    ping_reply.header.msg_type = STRING;
    ping_reply.data_len = msg_len;

    // Allocate buffer
    size_t total_size = sizeof(ping_reply.header) + sizeof(ping_reply.data_len) + msg_len;
    char *send_buffer = (char *)malloc(total_size);
    if (NULL == send_buffer)
    {
        perror("Memory allocation failed");
        return -1;
    }

    // put the data in the buffer to send
    memcpy(send_buffer, &ping_reply.header, sizeof(ping_reply.header));
    memcpy(send_buffer + sizeof(ping_reply.header), &ping_reply.data_len, sizeof(ping_reply.data_len));
    memcpy(send_buffer + sizeof(ping_reply.header) + sizeof(ping_reply.data_len), reply_msg, msg_len);

    ssize_t bytes_sent = send_all(client_sock, send_buffer, total_size);
    if (0 > bytes_sent)
    {
        perror("Send failed");
        free(send_buffer);
        return -1;
    }

    printf("PING Response Sent\n");
    free(send_buffer);
    return 0;
}

int handle_multiplication(int client_sock)
{
    p_mul_t mul_packet = {.header.msg_type = MUL};

    char *recvd_data = recv_expected(client_sock, sizeof(uint32_t));
    if (NULL == recvd_data)
    {
        perror("Receive Failed");
        return -1;
    }

    uint32_t num_recvd;
    memcpy(&num_recvd, recvd_data, sizeof(uint32_t));
    uint32_t num_in_correct_byte_order = ntohl(num_recvd);
    uint32_t result = num_in_correct_byte_order * 2;
    uint32_t result_net = htonl(result);

    // Build the response buffer
    mul_packet.data_len = sizeof(uint32_t);

    size_t total_size = sizeof(mul_packet.header) + sizeof(mul_packet.data_len) + sizeof(uint32_t);
    char *send_buffer = malloc(total_size);
    if (!send_buffer)
    {
        perror("Malloc failed");
        free(recvd_data);
        return -1;
    }

    memcpy(send_buffer, &mul_packet.header, sizeof(mul_packet.header));
    memcpy(send_buffer + sizeof(mul_packet.header), &mul_packet.data_len, sizeof(mul_packet.data_len));
    memcpy(send_buffer + sizeof(mul_packet.header) + sizeof(mul_packet.data_len), &result_net, sizeof(uint32_t));

    ssize_t bytes_sent = send_all(client_sock, send_buffer, total_size);
    if (bytes_sent < 0)
    {
        perror("Send failed");
        free(recvd_data);
        free(send_buffer);
        return -1;
    }

    printf("Multiplication Response Sent\n");
    free(recvd_data);
    free(send_buffer);
    return 0;
}

int handle_upload(int client_sock)
{
    printf("Inside handle upload function\n");
    char *recvd_data_len = recv_expected(client_sock, sizeof(uint32_t));
    if (NULL == recvd_data_len)
    {
        perror("Receive Failed");
        return -1;
    }

    uint32_t net_file_size;
    memcpy(&net_file_size, recvd_data_len, sizeof(uint32_t));
    free(recvd_data_len);

    uint32_t file_size = ntohl(net_file_size);
    printf("File Size Post HTONL: %d\n", file_size);
    char *file_contents = recv_expected(client_sock, file_size);
    if (file_contents == NULL)
    {
        perror("Failed to receive file contents");
        return -1;
    }
    printf("File Contents (Pre split):%s\n", file_contents);

    // Find the delimiter '|' to separate filename and content
    char *delimiter_pos = strchr(file_contents, '|');
    if (NULL == delimiter_pos)
    {
        fprintf(stderr, "Delimiter not found in upload data.\n");
        free(file_contents);
        return -1;
    }

    // Split into filename and content based on the delimiter
    *delimiter_pos = '\0'; // Null-terminate the filename
    char *filename = file_contents;
    char *content = delimiter_pos + 1; // Content starts after the delimiter

    if (filename && content)
    {
        // Construct the full file path
        int fd;
        char full_path[BUFFER];
        snprintf(full_path, sizeof(full_path), "%s%s", BASE_PATH, filename);

        // Open file for writing (create file if it doesn't exist)
        fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open with write-only, create, and truncate flags
        if (fd == -1)
        {
            perror("Error opening file");
            free(file_contents);
            return -1;
        }

        // Write contents to the file using write system call
        ssize_t content_size = file_size - (content - file_contents);
        ssize_t bytes_written = write(fd, content, content_size);

        if (bytes_written == -1)
        {
            perror("Error writing to file");
            close(fd);
            free(file_contents);
            return -1;
        }

        // Close the file descriptor after writing
        if (close(fd) == -1)
        {
            perror("Error closing file");
            free(file_contents);
            return -1;
        }

        printf("Write Successful\n");
        free(file_contents);
    }
    else
    {
        fprintf(stderr, "Invalid upload format received.\n");
        free(file_contents);
        return -1;
    }

    // Send reply
    p_upload_t upload_packet = {.header.msg_type = UPLOAD};
    uint8_t *success_msg = (uint8_t *)"Upload Successful";
    upload_packet.data = success_msg;
    upload_packet.data_len = strlen((const char *)success_msg) + 1;

    ssize_t total_size = sizeof(upload_packet.header) + sizeof(upload_packet.data_len) + upload_packet.data_len;
    char *send_buffer = malloc(total_size);
    if (!send_buffer)
    {
        perror("Malloc failed");
        return -1;
    }

    // Populate the buffer with the packet data
    memcpy(send_buffer, &upload_packet.header, sizeof(upload_packet.header));
    memcpy(send_buffer + sizeof(upload_packet.header), &upload_packet.data_len, sizeof(upload_packet.data_len));
    memcpy(send_buffer + sizeof(upload_packet.header) + sizeof(upload_packet.data_len), upload_packet.data, upload_packet.data_len);

    ssize_t bytes_sent = send_all(client_sock, send_buffer, total_size);
    if (0 > bytes_sent)
    {
        perror("Send failed");
        free(send_buffer);
        return -1;
    }

    printf("Upload Response Sent\n");
    free(send_buffer);
    return 0;
}

void ctrlc_handler(int dummy)
{
    keep_running = 0;
}
