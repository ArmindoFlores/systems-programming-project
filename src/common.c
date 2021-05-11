#include "common.h"
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>


static size_t min(size_t i, size_t j) { return i > j ? j : i; }

int recvall(int socket, char *buffer, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t recvd;
        if (buffer != NULL)
            recvd = recv(socket, buffer+total, (n - total), 0);
        else {
            char b[256];
            recvd = recv(socket, b, min(n - total, sizeof(b)), 0);
        }

        if (recvd == -1) // An error occurred
            return errno;
        else if (recvd == 0) // Client disconnected
            return 1;
        total += recvd;
    }
    return 0;
}

int sendall(int socket, char *buffer, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t sent = send(socket, buffer+total, (n - total), 0);
        if (sent == -1) // An error occurred
            return errno;
        else if (sent == 0) // Client disconnected
            return 1;
        total += sent;
    }
    return 0;
}