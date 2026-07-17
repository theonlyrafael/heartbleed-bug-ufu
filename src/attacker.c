#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9090
#define DECLARED_SIZE 256

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static ssize_t send_all(int socket_fd, const void *buffer, size_t size) {
    size_t sent = 0;

    while (sent < size) {
        ssize_t result = send(
            socket_fd,
            (const char *)buffer + sent,
            size - sent,
            0
        );

        if (result <= 0) {
            return result;
        }

        sent += (size_t)result;
    }

    return (ssize_t)sent;
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

    const char payload[] = "PING";
    uint16_t payload_size = (uint16_t)strlen(payload);
    uint16_t declared_size_network = htons(DECLARED_SIZE);
    uint16_t payload_size_network = htons(payload_size);

    if (send_all(
            socket_fd,
            &declared_size_network,
            sizeof(declared_size_network)
        ) != sizeof(declared_size_network)) {
        close(socket_fd);
        fail("Erro ao enviar o tamanho declarado");
    }

    if (send_all(
            socket_fd,
            &payload_size_network,
            sizeof(payload_size_network)
        ) != sizeof(payload_size_network)) {
        close(socket_fd);
        fail("Erro ao enviar o tamanho real");
    }

    if (send_all(socket_fd, payload, payload_size) != payload_size) {
        close(socket_fd);
        fail("Erro ao enviar o payload");
    }

    printf("Solicitacao heartbeat enviada.\n");
    printf("Tamanho real: %u bytes\n", payload_size);
    printf("Tamanho declarado: %d bytes\n", DECLARED_SIZE);

    close(socket_fd);

    return EXIT_SUCCESS;
}