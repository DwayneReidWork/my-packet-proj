#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
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
int handle_run_cmd(int client_sock, p_cmd_t *cmd_pack, char *cmd);
int handle_upload(int client_sock, p_upload_t *upload_pack);
int receive_packet(int client_sock, p_header_t *header, uint8_t **dest);
int serializeListPacket(p_list_files_t *pckt, uint8_t **buff);
int validate_socket(int client_sock);
void ctrlc_handler(int dummy);
volatile sig_atomic_t keep_running = 0;

int server_setup()
{
    // Signal Handler Stuff
    struct sigaction sa;
    sa.sa_handler = ctrlc_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

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

int receive_packet(int client_sock, p_header_t *header, uint8_t **dest)
{
    if (-1 == validate_socket(client_sock))
    {
        perror("Socket Error!");
        return -1;
    }

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
    if (-1 == validate_socket(client_sock))
    {
        perror("Socket Error!");
        return -1;
    }

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
    validate_socket(client_sock);

    if (NULL == mulpack)
    {
        perror("Mul Packet Error Error");
        return -1;
    }

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
    if (-1 == validate_socket(client_sock))
    {
        perror("Socket Error!");
        return -1;
    }

    if (NULL == upload_pack)
    {
        perror("Upload Packet Error Error");
        return -1;
    }

    p_header_t header = upload_pack->header;
    uint32_t payload_len = ntohl(header.p_len);

    char *filename = (char *)upload_pack->upload_data;
    char *delimiter_pos = strchr(filename, '|');

    if (NULL == delimiter_pos)
    {
        fprintf(stderr, "Delimiter not found in upload data.\n");
        return -1;
    }
    // set delimeter to null char
    *delimiter_pos = '\0';
    // increment ptr to content
    char *content = delimiter_pos + 1;

    size_t filename_len = strlen(filename) + 1;
    size_t content_size = payload_len - filename_len;

    if (0 == strlen(filename) || 0 == content_size)
    {
        fprintf(stderr, "Filename or content missing\n");
        return -1;
    }

    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_PATH, filename);

    int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (-1 == fd)
    {
        perror("Error opening file");
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
            return -1;
        }
        bytes_written += written;
    }

    close(fd);
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
    validate_socket(client_sock);
    // Buffer to store list
    uint8_t *reply_buff = NULL;
    // reate base for packet
    p_list_files_t reply_file_list = {0};
    reply_file_list.header.p_len = sizeof(reply_file_list.file_count);

    // Dirent to get list of files
    struct dirent *ent;
    reply_file_list.file_count = 0;
    DIR *dir = opendir(BASE_PATH);

    if (NULL == dir)
    {
        perror("Error opening directory");
        return -1;
    }
    // Loop through directory
    while (NULL != (ent = readdir(dir)))
    {
        // if the file found is a file and not a directory
        if (ent->d_type == DT_REG)
        {
            // realloc makes space for the new pointer for name
            reply_file_list.FileName = realloc(reply_file_list.FileName, (reply_file_list.file_count + 1) * sizeof(char *));

            // realloc makes space for the new pointer for name length
            reply_file_list.FileNameLen = realloc(reply_file_list.FileNameLen, (reply_file_list.file_count + 1) * sizeof(uint8_t));

            // string duplicate -> copy name into struct
            char *temp = strdup(ent->d_name);

            reply_file_list.FileName[reply_file_list.file_count] = temp;
            // get name length assign to struct
            reply_file_list.FileNameLen[reply_file_list.file_count] = strlen(temp);

            // increment new packet size for every file found
            // add file name length plus pointer to it
            reply_file_list.header.p_len += (strlen(temp) + sizeof(uint8_t));

            // increment file name counter
            reply_file_list.file_count++;
        }
    }
    // Close dirent directory
    closedir(dir);
    // put together packet
    reply_file_list.header.msg_type = LIST_FILES;
    // get actual size of fata to send over
    int reply_size = serializeListPacket(&reply_file_list, &reply_buff);
    // Send data
    send_all(client_sock, reply_buff, reply_size);
    // Free Buffer
    free(reply_buff);
    printf("List Sent!\n");
    return 0;
}

int serializeListPacket(p_list_files_t *pckt, uint8_t **buff)
{
    if (NULL == pckt)
    {
        perror("Serialization packet error");
        return -1;
    }

    if (NULL != *buff)
    {
        perror("Serialization Buffer error");
        return -1;
    }

    // Allocate packet buffer
    size_t total_size = sizeof(pckt->header) + pckt->header.p_len;

    *buff = malloc(total_size);
    if (NULL == *buff)
    {
        perror("Malloc Failed");
        return -1;
    }
    // move pointer
    uint8_t *ptr = *buff;

    // Populate the header
    memcpy(ptr, &pckt->header.msg_type, sizeof(pckt->header.msg_type));
    // move ptr
    ptr += sizeof(pckt->header.msg_type);

    // convert endianness
    int p_len = htonl(pckt->header.p_len);
    // copy to buffer
    memcpy(ptr, &p_len, sizeof(pckt->header.p_len));
    // move pointer
    ptr += sizeof(pckt->header.p_len);

    // Populate the number of files
    memcpy(ptr, &pckt->file_count, sizeof(pckt->file_count));
    // move pointer
    ptr += sizeof(pckt->file_count);

    // For each file
    // Add the file name length
    // Add the file name itself

    for (size_t i = 0; i < pckt->file_count; i++)
    {
        uint8_t len = pckt->FileNameLen[i];
        // Copy file length to buffer
        memcpy(ptr, &len, sizeof(uint8_t));
        // move pointer
        ptr += sizeof(uint8_t);
        // copy matching file name to buffer
        memcpy(ptr, pckt->FileName[i], len);
        // move ptr
        ptr += len;
    }
    // return total size of buffer (which is used to send it back in the calling function)
    return total_size;
}

int validate_socket(int client_sock)
{
    if (-1 == client_sock)
    {
        return -1;
    }
    return 0;
}

void ctrlc_handler(int dummy)
{
    const char *error_message = "\nCTRL+C signal received.\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    keep_running = -1;
}

int handle_run_cmd(int client_sock, p_cmd_t *cmd_pack, char *cmd)
{
    printf("Inside Handle_run_md\n");
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
        //  close listening side
        printf("Inside Child Process\n");
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        dup2(fd[1], STDERR_FILENO);
        close(fd[1]);
        printf("Right before execlp.\n");
        execlp("sh", "sh", "-c", cmd, (char *)NULL);
    }
    else
    {
        // Parent process
        // Close write end
        close(fd[1]);
        char buffer[1024] = {0};
        ssize_t bytes_read;
        size_t total_bytes = 0;

        while (0 < (bytes_read = read(fd[0], buffer, sizeof(buffer))))
        {
            total_bytes += bytes_read;
        }

        printf("Buffer:\n%s\nTotal Bytes Read: %ld\n", buffer, total_bytes);
        size_t total_size = sizeof(p_cmd_t) + total_bytes;
        p_cmd_t *cmd_reply = malloc(total_size);
        if (NULL == cmd_reply)
        {
            perror("Malloc failed");
            return -1;
        }

        cmd_reply->header.msg_type = RUN_CMD;
        cmd_reply->header.p_len = htonl(total_bytes);
        memcpy(cmd_reply->cmd, buffer, total_bytes);

        if (-1 == send_all(client_sock, (uint8_t *)cmd_reply, total_size))
        {
            perror("Sendall Failed.");
            return -1;
        }

        free(cmd_reply);
        close(fd[0]);
        waitpid(pid, NULL, 0);
    }
    return 0;
}
//         // //Hex Print
//         // printf("Sent Data (Hex):\n");
//         // for (ssize_t i = 0; i < (sizeof(p_cmd_t) + size); i++)
//         //     printf("%02X ", ((uint8_t *)reply_pack)[i]);
//         // printf("\n");