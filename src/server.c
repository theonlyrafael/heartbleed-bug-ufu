#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SERVER_PORT 9090
#define CONNECTION_BACKLOG 5

#define MAX_PAYLOAD_SIZE 64
#define MAX_DECLARED_SIZE 512
#define HEAP_REGION_SIZE 768

#define HEAP_BLOCK_COUNT 12
#define MIN_FAILED_ATTEMPTS 3
#define LEAK_PROBABILITY_DIVISOR 4

#define SECRET_VALUE "SEGREDO: ELES ESTAO DE OLHO EM NOS!"

static unsigned int request_number = 0;

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

static size_t random_between(size_t minimum, size_t maximum) {
    if (maximum <= minimum) {
        return minimum;
    }

    size_t interval = maximum - minimum + 1;

    return minimum + ((size_t)rand() % interval);
}

static void prepare_heap(void) {
    void *blocks[HEAP_BLOCK_COUNT] = {0};

    for (size_t i = 0; i < HEAP_BLOCK_COUNT; i++) {
        size_t block_size = 48 + (i * 37);

        blocks[i] = malloc(block_size);

        if (blocks[i] == NULL) {
            fail("Erro ao preparar o heap");
        }

        memset(
            blocks[i],
            (int)('A' + (i % 26)),
            block_size
        );
    }

    /*
     * Libera os blocos de forma alternada para simular
     * fragmentacao e reutilizacao da memoria.
     */
    for (size_t i = 0; i < HEAP_BLOCK_COUNT; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
    }

    void *temporary_blocks[4] = {0};

    for (size_t i = 0; i < 4; i++) {
        size_t block_size = 80 + (i * 45);

        temporary_blocks[i] = malloc(block_size);

        if (temporary_blocks[i] == NULL) {
            fail("Erro ao realocar blocos temporarios");
        }

        memset(temporary_blocks[i], 'X', block_size);
    }

    for (size_t i = 0; i < 4; i++) {
        free(temporary_blocks[i]);
    }

    for (size_t i = 1; i < HEAP_BLOCK_COUNT; i += 2) {
        free(blocks[i]);
    }
}

static size_t choose_secret_offset(
    uint16_t declared_size,
    uint16_t payload_size,
    size_t secret_size
) {
    size_t hidden_minimum = (size_t)declared_size + 32;
    size_t hidden_maximum = HEAP_REGION_SIZE - secret_size - 1;

    /*
     * Nas primeiras tentativas, o segredo fica obrigatoriamente
     * fora da regiao devolvida ao atacante.
     */
    if (request_number <= MIN_FAILED_ATTEMPTS) {
        return random_between(
            hidden_minimum,
            hidden_maximum
        );
    }

    /*
     * Depois das tentativas iniciais, existe aproximadamente
     * 25% de chance de o segredo ficar na regiao vazada.
     */
    int should_leak =
        rand() % LEAK_PROBABILITY_DIVISOR == 0;

    if (!should_leak) {
        return random_between(
            hidden_minimum,
            hidden_maximum
        );
    }

    size_t leaked_minimum = (size_t)payload_size + 32;
    size_t leaked_maximum =
        (size_t)declared_size - secret_size;

    return random_between(
        leaked_minimum,
        leaked_maximum
    );
}

static int handle_client(int client_fd) {
    uint16_t declared_size_network;
    uint16_t payload_size_network;

    unsigned char *heap_region = NULL;
    char *secret = NULL;

    int result = EXIT_FAILURE;

    if (receive_all(
            client_fd,
            &declared_size_network,
            sizeof(declared_size_network)
        ) != sizeof(declared_size_network)) {
        fprintf(
            stderr,
            "Erro ao receber o tamanho declarado.\n"
        );

        goto cleanup;
    }

    if (receive_all(
            client_fd,
            &payload_size_network,
            sizeof(payload_size_network)
        ) != sizeof(payload_size_network)) {
        fprintf(
            stderr,
            "Erro ao receber o tamanho real.\n"
        );

        goto cleanup;
    }

    uint16_t declared_size =
        ntohs(declared_size_network);

    uint16_t payload_size =
        ntohs(payload_size_network);

    if (
        payload_size == 0 ||
        payload_size > MAX_PAYLOAD_SIZE
    ) {
        fprintf(
            stderr,
            "Tamanho real do payload invalido.\n"
        );

        goto cleanup;
    }

    if (
        declared_size == 0 ||
        declared_size > MAX_DECLARED_SIZE
    ) {
        fprintf(
            stderr,
            "Tamanho declarado fora do limite da simulacao.\n"
        );

        goto cleanup;
    }

    prepare_heap();

    heap_region = malloc(HEAP_REGION_SIZE);

    if (heap_region == NULL) {
        fprintf(
            stderr,
            "Erro ao alocar a regiao do heap.\n"
        );

        goto cleanup;
    }

    /*
     * Preenche a regiao com bytes residuais variados.
     * Na saida textual, bytes nao imprimiveis aparecem como pontos.
     */
    for (size_t i = 0; i < HEAP_REGION_SIZE; i++) {
        heap_region[i] = (unsigned char)(rand() % 256);
    }

    if (receive_all(
            client_fd,
            heap_region,
            payload_size
        ) != payload_size) {
        fprintf(
            stderr,
            "Erro ao receber o payload.\n"
        );

        goto cleanup;
    }

    heap_region[payload_size] = '\0';

    size_t secret_size =
        strlen(SECRET_VALUE) + 1;

    secret = malloc(secret_size);

    if (secret == NULL) {
        fprintf(
            stderr,
            "Erro ao alocar a string secreta.\n"
        );

        goto cleanup;
    }

    memcpy(
        secret,
        SECRET_VALUE,
        secret_size
    );

    request_number++;

    size_t secret_offset = choose_secret_offset(
        declared_size,
        payload_size,
        secret_size
    );

    /*
     * Insere uma separacao visual antes da string secreta.
     * O limite minimo usado no deslocamento garante que o indice
     * anterior seja valido.
     */
    heap_region[secret_offset - 1] = ' ';

    memcpy(
        heap_region + secret_offset,
        secret,
        secret_size
    );

    int secret_inside_response =
        secret_offset + secret_size <= declared_size;

    printf(
        "\nTentativa recebida pelo servidor: %u\n",
        request_number
    );

    printf(
        "Payload recebido: %s\n",
        (char *)heap_region
    );

    printf(
        "Tamanho real: %u bytes\n",
        payload_size
    );

    printf(
        "Tamanho declarado: %u bytes\n",
        declared_size
    );

    printf(
        "Deslocamento do segredo no heap: %zu\n",
        secret_offset
    );

    if (secret_inside_response) {
        printf(
            "O segredo ficou por acaso dentro da regiao enviada.\n"
        );
    } else {
        printf(
            "O segredo ficou fora da regiao enviada.\n"
        );
    }

    /*
     * Vulnerabilidade intencional:
     * o servidor deveria devolver apenas payload_size bytes,
     * mas confia no tamanho declarado pelo atacante.
     */
    if (send_all(
            client_fd,
            heap_region,
            declared_size
        ) != declared_size) {
        fprintf(
            stderr,
            "Erro ao enviar a resposta vulneravel.\n"
        );

        goto cleanup;
    }

    printf(
        "Servidor enviou %u bytes para um payload de apenas %u bytes.\n",
        declared_size,
        payload_size
    );

    result = EXIT_SUCCESS;

cleanup:
    free(secret);
    free(heap_region);

    return result;
}

int main(void) {
    srand(
        (unsigned int)time(NULL) ^
        (unsigned int)getpid()
    );

    int server_fd = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

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
    server_address.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    server_address.sin_port =
        htons(SERVER_PORT);

    if (bind(
            server_fd,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == -1) {
        close(server_fd);
        fail("Erro ao associar o socket");
    }

    if (
        listen(
            server_fd,
            CONNECTION_BACKLOG
        ) == -1
    ) {
        close(server_fd);
        fail("Erro ao iniciar a escuta");
    }

    printf(
        "Servidor aguardando conexoes em 127.0.0.1:%d...\n",
        SERVER_PORT
    );

    while (1) {
        struct sockaddr_in client_address = {0};

        socklen_t client_address_length =
            sizeof(client_address);

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

        printf(
            "Conexao encerrada. Aguardando nova tentativa...\n"
        );
    }

    close(server_fd);

    return EXIT_SUCCESS;
}