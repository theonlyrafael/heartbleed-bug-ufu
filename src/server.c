#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 9090
#define CONNECTION_BACKLOG 1

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd == -1) {
        fail("Erro ao criar o socket");
    }

    printf("Socket do servidor criado para a porta %d.\n", SERVER_PORT);

    close(server_fd);

    return EXIT_SUCCESS;
}