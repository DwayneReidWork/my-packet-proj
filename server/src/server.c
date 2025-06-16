#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include "../inc/netcomm.h"
#include "../inc/packet.h"
#include "../inc/server.h"
#include "../inc/signal.h"

#define CMD_TIMEOUT 5
#define DEFAULT_PORT 1234

int handle_client(int client_sock);

int server_setup(int port)
{
    if (0 != signal_handler_setup())
    {
        perror("Signal Handler Error");
        return -1;
    }
    // Server Setup Stuff
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
    address.sin_port = htons(port);

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

    printf("Listening on port %d.\n", port);

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
    if (-1 == validate_socket(client_sock))
    {
        perror("Socket Error!");
        return -1;
    }

    uint8_t *data = NULL;
    p_header_t header;

    // Loop to continue receiving packets
    while (-1 != keep_running)
    {
        switch (receive_packet(client_sock, &header, &data))
        {
        case PING:
            handle_ping(client_sock);
            break;
        case CONNECT:
            printf("Connection Request Received!\n");
            p_conn_t conn_pack;
            memcpy(&conn_pack.header, &header, sizeof(header));
            memcpy(&conn_pack.port, data, sizeof(conn_pack.port));
            printf("Data Received: %d\n", ntohl(conn_pack.port));

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
            if (NULL == upload_pack)
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
                return -1;
            }

            free(upload_pack);
            break;
        case LIST_FILES:
            printf("Received List File Packet.\n");
            handle_list_files(client_sock);
            break;
        case RUN_CMD:
            printf("Received Cmd Packet.\n");
            uint32_t cmd_len = ntohl(header.p_len);
            p_cmd_t *cmd_pack = malloc(sizeof(p_cmd_t) + cmd_len + 1);

            if (NULL == cmd_pack)
            {
                perror("malloc failed");
                free(data);
                return -1;
            }
            // copy header into pack
            memcpy(&cmd_pack->header, &header, sizeof(header));
            // memcpy the command to the pack
            memcpy(cmd_pack->cmd, data, cmd_len);
            // null terminate cmd
            cmd_pack->cmd[cmd_len] = '\0';
            // pass cmd to be handled
            printf("Cmd Received: %s\n", cmd_pack->cmd);
            handle_run_cmd(client_sock, cmd_pack, cmd_pack->cmd);
            break;
        default:
            printf("Invalid Packet Type Received\n");
            break;
        }
    }
    free(data);
    close(client_sock);
    return 0;
}

int validate_socket(int client_sock)
{
    if (-1 == client_sock)
    {
        return -1;
    }
    return 0;
}

int handle_run_cmd(int client_sock, p_cmd_t *cmd_pack, char *cmd)
{
    if (-1 == validate_socket(client_sock))
    {
        perror("Socket Error!");
        return -1;
    }

    int fd[2];
    if (-1 == pipe(fd))
    {
        perror("Pipe Creation Error.");
        return -1;
    }

    pid_t pid = fork();

    if (-1 == pid)
    {
        perror("Error Forking.");
        return -1;
    }
    else if (0 == pid)
    {
        // Child Process
        // close listening side
        close(fd[0]);
        // redirect stderr and stdout to the pipe
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        // close pipe
        close(fd[1]);
        // run the command via the shell
        // execlp closes program when done running
        execlp("sh", "sh", "-c", cmd, (char *)NULL);
        // will only hit here if error occurs
        perror("execlp failed");
        exit(1);
    }
    else
    {
        // Parent process
        child_pid = pid;
        // start timer for timeout
        alarm(CMD_TIMEOUT);
        // Close write end
        close(fd[1]);
        char buffer[1024] = {0};
        char *output = NULL;
        ssize_t bytes_read = 0;
        size_t total_bytes = 0;

        while (0 < (bytes_read = read(fd[0], buffer, sizeof(buffer))))
        {
            // reallocate size for response
            char *new_output = realloc(output, total_bytes + bytes_read + 1);
            if (NULL == new_output)
            {
                perror("Realloc failed");
                free(output);
                close(fd[0]);
                return -1;
            }
            // copy output to buffer
            output = new_output;
            memcpy(output + total_bytes, buffer, bytes_read);
            total_bytes += bytes_read;
            // null terminate
            output[total_bytes] = '\0';
        }

        size_t total_size = sizeof(p_cmd_t) + total_bytes;
        p_cmd_t *cmd_reply = malloc(total_size);
        if (NULL == cmd_reply)
        {
            perror("Malloc failed");
            free(output);
            return -1;
        }
        // create reply packet, copy data from buffer to cmd
        cmd_reply->header.msg_type = RUN_CMD;
        cmd_reply->header.p_len = htonl(total_bytes);
        memcpy(cmd_reply->cmd, output, total_bytes);

        if (-1 == send_all(client_sock, (uint8_t *)cmd_reply, total_size))
        {
            perror("Sendall Failed.");
            return -1;
        }

        free(output);
        free(cmd_reply);
        close(fd[0]);
        // wait for child process to be reaped
        waitpid(pid, NULL, 0);
        // cancel alarm if finished.
        alarm(0);
        // reset atomic
        child_pid = -1;
    }

    return 0;
}