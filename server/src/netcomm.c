#include <stdlib.h>
#include <sys/socket.h>

// Function to send all data, returns 0 on success, -1 on failure
int send_all(int sock, const char *data, size_t data_len)
{
    size_t total_sent_bits = 0;
    while (total_sent_bits < data_len)
    {
        ssize_t sent_bits = send(sock, data + total_sent_bits, data_len - total_sent_bits, 0);
        if (sent_bits == -1)
        {
            return -1;
        }
        total_sent_bits += sent_bits;
    }
    return 0;
}

// Function to receive expected data, returns a pointer to the received data buffer
char *recv_expected(int sock, size_t expected_len)
{
    char *recvd_bytes = (char *)malloc(expected_len);
    if (recvd_bytes == NULL)
    {
        return NULL; // Memory allocation failed
    }

    size_t recvd_len = 0;
    while (recvd_len < expected_len)
    {
        ssize_t received = recv(sock, recvd_bytes + recvd_len, expected_len - recvd_len, 0);
        if (received == -1)
        {
            free(recvd_bytes);
            return NULL; // Return NULL if recv fails
        }
        recvd_len += received;
    }
    return recvd_bytes;
}