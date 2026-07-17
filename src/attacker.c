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
 * Deve ser exatamente a mesma string definida no servidor.
 * Isso só será usado para teste.
 */
#define SECRET_VALUE "SEGREDO: ELES ESTAO DE OLHO EM NOS!"

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

static int contains_secret(
    const unsigned char *buffer,
    size_t buffer_size,
    const char *secret
) {
    size_t secret_size = strlen(secret);

    if (secret_size > buffer_size) {
        return 0;
    }

    for (size_t i = 0; i <= buffer_size - secret_size; i++) {
        if (memcmp(buffer + i, secret, secret_size) == 0) {
            return 1;
        }
    }

    return 0;
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

static int execute_attempt(unsigned char *response) {
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

    return contains_secret(response, DECLARED_SIZE, SECRET_VALUE);
}

int main(void) {
    unsigned char *response = malloc(DECLARED_SIZE);

    if (response == NULL) {
        fail("Erro ao alocar a resposta");
    }

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf("Tentativa %d de %d...\n", attempt, MAX_ATTEMPTS);

        if (execute_attempt(response)) {
            printf("\nString secreta encontrada na tentativa %d!\n", attempt);
            printf("Segredo vazado: %s\n", SECRET_VALUE);

            print_memory(response, DECLARED_SIZE);

            free(response);
            return EXIT_SUCCESS;
        }

        printf("String secreta ainda nao encontrada.\n");
    }

    printf("\nLimite de tentativas atingido sem encontrar o segredo.\n");

    free(response);

    return EXIT_FAILURE;
}