#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SERVER_PORT 9090
#define CONNECTION_BACKLOG 5
#define MAX_PAYLOAD_SIZE 64
#define MAX_DECLARED_SIZE 512
#define HEAP_BLOCK_COUNT 8
#define SECRET_VALUE "SEGREDO: ELES ESTAO DE OLHO EM NOS!"

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

static void prepare_heap(void) {
    void *blocks[HEAP_BLOCK_COUNT];

    for (size_t i = 0; i < HEAP_BLOCK_COUNT; i++) {
        size_t block_size = 128 + (i * 64);

        blocks[i] = malloc(block_size);

        if (blocks[i] == NULL) {
            fail("Erro ao preparar o heap");
        }

        memset(blocks[i], (int)('A' + i), block_size);
    }

    for (size_t i = 0; i < HEAP_BLOCK_COUNT; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
    }

    for (size_t i = 1; i < HEAP_BLOCK_COUNT; i += 2) {
        free(blocks[i]);
    }
}

static int handle_client(int client_fd) {
    uint16_t declared_size_network;
    uint16_t payload_size_network;

    char *payload = NULL;
    char *secret = NULL;
    int result = EXIT_FAILURE;

    prepare_heap();

    if (receive_all(
            client_fd,
            &declared_size_network,
            sizeof(declared_size_network)
        ) != sizeof(declared_size_network)) {
        fprintf(stderr, "Erro ao receber o tamanho declarado.\n");
        goto cleanup;
    }

    if (receive_all(
            client_fd,
            &payload_size_network,
            sizeof(payload_size_network)
        ) != sizeof(payload_size_network)) {
        fprintf(stderr, "Erro ao receber o tamanho real.\n");
        goto cleanup;
    }

    uint16_t declared_size = ntohs(declared_size_network);
    uint16_t payload_size = ntohs(payload_size_network);

    if (payload_size == 0 || payload_size > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "Tamanho real do payload invalido.\n");
        goto cleanup;
    }

    if (declared_size == 0 || declared_size > MAX_DECLARED_SIZE) {
        fprintf(stderr, "Tamanho declarado fora do limite da simulacao.\n");
        goto cleanup;
    }

    payload = malloc((size_t)payload_size + 1);

    if (payload == NULL) {
        fprintf(stderr, "Erro ao alocar o payload.\n");
        goto cleanup;
    }

    if (receive_all(client_fd, payload, payload_size) != payload_size) {
        fprintf(stderr, "Erro ao receber o payload.\n");
        goto cleanup;
    }

    payload[payload_size] = '\0';

    size_t secret_size = strlen(SECRET_VALUE) + 1;

    secret = malloc(secret_size);

    if (secret == NULL) {
        fprintf(stderr, "Erro ao alocar a string secreta.\n");
        goto cleanup;
    }

    memcpy(secret, SECRET_VALUE, secret_size);

    printf("\nPayload recebido: %s\n", payload);
    printf("Tamanho real: %u bytes\n", payload_size);
    printf("Tamanho declarado: %u bytes\n", declared_size);
    printf("String secreta armazenada no heap.\n");

    /*
     * Vulnerabilidade intencional:
     * o servidor envia declared_size bytes a partir de um buffer
     * alocado somente para payload_size + 1 bytes.
     */
    if (send_all(client_fd, payload, declared_size) != declared_size) {
        fprintf(stderr, "Erro ao enviar a resposta vulneravel.\n");
        goto cleanup;
    }

    printf(
        "Servidor enviou %u bytes a partir do buffer pequeno.\n",
        declared_size
    );

    result = EXIT_SUCCESS;

cleanup:
    free(secret);
    free(payload);

    return result;
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
        "Servidor aguardando conexoes em 127.0.0.1:%d...\n",
        SERVER_PORT
    );

    while (1) {
        struct sockaddr_in client_address = {0};
        socklen_t client_address_length = sizeof(client_address);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_address,
            &client_address_length
        );

        if (client_fd == -1) {
            perror("Erro ao aceitar a conexao");
            continue;
        }

        printf("\nNova conexao aceita.\n");

        handle_client(client_fd);
        close(client_fd);

        printf("Conexao encerrada. Aguardando nova tentativa...\n");
    }

    close(server_fd);

    return EXIT_SUCCESS;
}