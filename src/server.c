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

    int reuse_address = 1;

    if (setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address,
            sizeof(reuse_address)
        ) == -1) {
        close(server_fd);
        fail("Erro ao configurar o socket");
    }

    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_address.sin_port = htons(SERVER_PORT);

    if (bind(
            server_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == -1) {
        close(server_fd);
        fail("Erro ao associar o socket");
    }

    if (listen(server_fd, CONNECTION_BACKLOG) == -1) {
        close(server_fd);
        fail("Erro ao iniciar a escuta");
    }

    printf(
        "Servidor configurado em 127.0.0.1:%d.\n",
        SERVER_PORT
    );

    close(server_fd);

    return EXIT_SUCCESS;
}