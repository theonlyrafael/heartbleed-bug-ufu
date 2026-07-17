#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 9090
#define CONNECTION_BACKLOG 1
#define MAX_PAYLOAD_SIZE 64
#define MAX_DECLARED_SIZE 512

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static ssize_t receive_all(int socket_fd, void *buffer, size_t size) {
    size_t received = 0;

    while (received < size) {
        ssize_t result = recv(
            socket_fd,
            (char *)buffer + received,
            size - received,
            0
        );

        if (result <= 0) {
            return result;
        }

        received += (size_t)result;
    }

    return (ssize_t)received;
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

    printf("Servidor aguardando conexao em 127.0.0.1:%d...\n", SERVER_PORT);

    struct sockaddr_in client_address = {0};
    socklen_t client_address_length = sizeof(client_address);

    int client_fd = accept(
        server_fd,
        (struct sockaddr *)&client_address,
        &client_address_length
    );

    if (client_fd == -1) {
        close(server_fd);
        fail("Erro ao aceitar a conexao");
    }

    uint16_t declared_size_network;
    uint16_t payload_size_network;

    if (receive_all(
            client_fd,
            &declared_size_network,
            sizeof(declared_size_network)
        ) != sizeof(declared_size_network)) {
        close(client_fd);
        close(server_fd);
        fail("Erro ao receber o tamanho declarado");
    }

    if (receive_all(
            client_fd,
            &payload_size_network,
            sizeof(payload_size_network)
        ) != sizeof(payload_size_network)) {
        close(client_fd);
        close(server_fd);
        fail("Erro ao receber o tamanho real");
    }

    uint16_t declared_size = ntohs(declared_size_network);
    uint16_t payload_size = ntohs(payload_size_network);

    if (payload_size == 0 || payload_size > MAX_PAYLOAD_SIZE) {
        close(client_fd);
        close(server_fd);
        fprintf(stderr, "Tamanho real do payload invalido.\n");
        return EXIT_FAILURE;
    }

    if (declared_size == 0 || declared_size > MAX_DECLARED_SIZE) {
        close(client_fd);
        close(server_fd);
        fprintf(stderr, "Tamanho declarado fora do limite da simulacao.\n");
        return EXIT_FAILURE;
    }

    char *payload = malloc((size_t)payload_size + 1);

    if (payload == NULL) {
        close(client_fd);
        close(server_fd);
        fail("Erro ao alocar o payload");
    }

    if (receive_all(client_fd, payload, payload_size) != payload_size) {
        free(payload);
        close(client_fd);
        close(server_fd);
        fail("Erro ao receber o payload");
    }

    payload[payload_size] = '\0';

    printf("Payload recebido: %s\n", payload);
    printf("Tamanho real: %u bytes\n", payload_size);
    printf("Tamanho declarado: %u bytes\n", declared_size);

    /*
     * Vulnerabilidade intencional:
     * o buffer possui apenas payload_size + 1 bytes, mas o servidor
     * envia declared_size bytes a partir do mesmo endereco.
     */
    if (send_all(client_fd, payload, declared_size) != declared_size) {
        free(payload);
        close(client_fd);
        close(server_fd);
        fail("Erro ao enviar a resposta vulneravel");
    }

    printf("Servidor enviou %u bytes a partir do buffer pequeno.\n", declared_size);

    free(payload);
    close(client_fd);
    close(server_fd);

    return EXIT_SUCCESS;
}