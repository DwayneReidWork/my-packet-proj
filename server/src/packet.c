#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../inc/packet.h"
#include "../inc/netcomm.h"
#include "../inc/server.h"

#define BASE_PATH "server/for_uploads/"

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