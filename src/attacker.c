#include <arpa/inet.h>
#include <ctype.h>
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

static void print_memory(const unsigned char *buffer, size_t size) {
    printf("\nConteudo recebido em formato hexadecimal:\n");

    for (size_t i = 0; i < size; i++) {
        printf("%02x ", buffer[i]);

        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }

    printf("\nRepresentacao textual:\n");

    for (size_t i = 0; i < size; i++) {
        putchar(isprint(buffer[i]) ? buffer[i] : '.');
    }

    printf("\n");
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

    unsigned char *response = malloc(DECLARED_SIZE);

    if (response == NULL) {
        close(socket_fd);
        fail("Erro ao alocar a resposta");
    }

    if (receive_all(socket_fd, response, DECLARED_SIZE) != DECLARED_SIZE) {
        free(response);
        close(socket_fd);
        fail("Erro ao receber a resposta");
    }

    printf("Payload enviado: %s\n", payload);
    printf("Tamanho real enviado: %u bytes\n", payload_size);
    printf("Tamanho solicitado: %d bytes\n", DECLARED_SIZE);

    print_memory(response, DECLARED_SIZE);

    free(response);
    close(socket_fd);

    return EXIT_SUCCESS;
}