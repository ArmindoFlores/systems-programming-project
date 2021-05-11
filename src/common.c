#include "common.h"
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>


int recvall(int socket, char *buffer, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t recvd = recv(socket, buffer+total, (n - total), 0);
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