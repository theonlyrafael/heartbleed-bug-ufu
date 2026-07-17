#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9090

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

int main(void) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        fail("Erro ao criar o socket");
    }

    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) != 1) {
        close(socket_fd);
        fail("Erro ao configurar o endereco do servidor");
    }

    if (connect(
            socket_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == -1) {
        close(socket_fd);
        fail("Erro ao conectar ao servidor");
    }

    printf("Conectado ao servidor em %s:%d.\n", SERVER_IP, SERVER_PORT);

    close(socket_fd);

    return EXIT_SUCCESS;
}