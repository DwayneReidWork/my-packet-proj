#include <arpa/inet.h>
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
    signal(SIGINT, ctrlc_handler);

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
    printf("Recvd Packet Type Raw: %d\nBytes received: %ld\n", packet_type, bytes);

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
    // Receive data info
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
    // printf("Expected file size: %u bytes\n", file_size);
    char *file_contents = recv_expected(client_sock, file_size);
    file_contents[file_size] = '\0';

    // Parse and write data
    const char *delimiter = "|";
    char *filename = strtok(file_contents, delimiter);
    char *content = strtok(NULL, "");

    // printf("Filename: %s\nFile Contents: %s\n", filename, content);

    if (NULL != filename && NULL != content)
    {
        FILE *fp;
        char full_path[BUFFER];
        snprintf(full_path, sizeof(full_path), "%s%s", BASE_PATH, filename);
        fp = fopen(full_path, "w");
        if (NULL == fp)
        {
            perror("Error opening file");
            return 1;
        }
        fprintf(fp, "%s", content);
        if (fclose(fp) == EOF)
        {
            perror("Error closing file");
            return 1;
        }
        printf("Write Successful\n");

        // EVERYTHING ABOVE WORKS
        //  Send reply
        p_upload_t upload_packet = {.header.msg_type = UPLOAD};
        size_t total_size = sizeof(upload_packet.header) + sizeof(upload_packet.data_len) + sizeof(uint32_t);
        char *send_buffer = malloc(total_size);
        if (!send_buffer)
        {
            perror("Malloc failed");
            return -1;
        }

        memcpy(send_buffer, &upload_packet.header, sizeof(upload_packet.header));
        memcpy(send_buffer + sizeof(upload_packet.header), &upload_packet.data_len, sizeof(upload_packet.data_len));
        memcpy(send_buffer + sizeof(upload_packet.header) + sizeof(upload_packet.data_len), &content, sizeof(uint32_t));

        ssize_t bytes_sent = send_all(client_sock, send_buffer, total_size);
        if (bytes_sent < 0)
        {
            perror("Send failed");
            free(send_buffer);
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "Invalid upload format received.\n");
    }
    return 0;
}

void ctrlc_handler(int dummy)
{
    keep_running = 0;
}
