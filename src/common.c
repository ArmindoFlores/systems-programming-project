#include "common.h"
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>

int recvall(int socket, char *buffer, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t recvd = recv(socket, buffer+total, (n - total), 0);
        if (recvd == -1)
            return errno;
        total += recvd;
    }
    return 0;
}

int sendall(int socket, char *buffer, size_t n)
{
    size_t total = 0;
    while (total < n) {
        ssize_t sent = send(socket, buffer+total, (n - total), 0);
        if (sent == -1)
            return errno;
    }
    return 0;
}