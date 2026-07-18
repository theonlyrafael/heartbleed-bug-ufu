# Trabalho de Segurança da Informação - Heartbleed

> **Aviso acadêmico:** Prova de conceito desenvolvida individualmente para a disciplina de Segurança da Informação.

![C](https://img.shields.io/badge/C-Programming-00599C?style=flat)
![GCC](https://img.shields.io/badge/GCC-Compiler-A42E2B?style=flat)
![Make](https://img.shields.io/badge/Make-Build-427819?style=flat)
![POSIX Sockets](https://img.shields.io/badge/POSIX-Sockets-4B5563?style=flat)
![Status](https://img.shields.io/badge/Status-Em%20finaliza%C3%A7%C3%A3o-yellow?style=flat)

Este repositório contém a resolução do **terceiro** trabalho prático da disciplina de Segurança da Informação, do oitavo período do curso de Sistemas de Informação da Universidade Federal de Uberlândia (UFU). 

## Como o exercício foi montado

Este projeto implementa uma prova de conceito educacional inspirada na vulnerabilidade **Heartbleed**, uma falha de segurança crítica descoberta em 2014 na biblioteca criptográfica OpenSSL, demonstrando como a confiança em um tamanho informado pelo cliente pode causar a exposição indevida de dados armazenados na memória de um processo.

A implementação é formada por dois programas escritos em C:

- `server`: servidor vulnerável que recebe solicitações por meio de um socket TCP;
- `attacker`: cliente que envia solicitações manipuladas e analisa os bytes devolvidos pelo servidor.

Toda a comunicação é limitada ao endereço local:

```text
127.0.0.1:9090
```

Assim, a demonstração ocorre somente na própria máquina, sem disponibilizar o servidor vulnerável na rede externa.

## Os conceitos envolvidos

### Heartbleed

A vulnerabilidade Heartbleed original estava relacionada à extensão Heartbeat do protocolo TLS. O servidor confiava no tamanho declarado pelo cliente sem verificar se esse valor correspondia ao tamanho real da mensagem recebida.

Como consequência, o servidor podia devolver uma quantidade de bytes maior do que a mensagem enviada, expondo informações presentes na memória do processo.

Este projeto não implementa TLS nem reproduz integralmente a vulnerabilidade original do OpenSSL. Ele representa o mesmo erro fundamental em um ambiente controlado:

```text
tamanho declarado > tamanho real do payload
```

### Over-read

Um **over-read** ocorre quando um programa lê uma quantidade de memória maior do que aquela que deveria ser acessada para determinada informação.

Na prova de conceito, o atacante envia o payload:

```text
PING
```

O payload possui apenas 4 bytes, mas o atacante declara um tamanho de 256 bytes.

O servidor confia no tamanho declarado e envia 256 bytes da região de memória utilizada pela mensagem. Dessa forma, além de `PING`, são devolvidos bytes residuais e, eventualmente, a string sensível armazenada no heap.

### Heap

A memória utilizada durante o ataque é alocada dinamicamente por meio de `malloc()`.

Antes de processar cada solicitação, o servidor executa várias alocações e desalocações de blocos com tamanhos diferentes. Esse comportamento simula a atividade de memória de um processo real e atende ao requisito de preparar o heap antes do ataque.

A string sensível também é criada em memória dinâmica:

```c
#define SECRET_VALUE "SEGREDO: ELES ESTAO DE OLHO EM NOS!"
```

Nenhuma parte dessa string está presente no código do atacante.

### Sockets

A comunicação entre os dois processos utiliza sockets TCP da interface POSIX.

O servidor realiza as operações:

```text
socket → bind → listen → accept → recv → send
```

O atacante realiza:

```text
socket → connect → send → recv
```

## Tarefas realizadas

### 1. Implementação do servidor

O servidor foi configurado para:

- criar um socket TCP;
- aceitar conexões em `127.0.0.1:9090`;
- receber o tamanho declarado pelo atacante;
- receber o tamanho real do payload;
- receber o conteúdo da mensagem;
- alocar memória utilizando `malloc()`;
- preparar o heap com várias alocações e desalocações;
- armazenar uma string sensível na região de memória;
- responder utilizando o tamanho declarado, sem validar sua correspondência com o payload real;
- permanecer em execução para aceitar novas tentativas.

### 2. Implementação do atacante

O atacante foi configurado para:

- conectar-se ao servidor local;
- enviar um payload de 4 bytes;
- declarar falsamente um tamanho de 256 bytes;
- receber os 256 bytes devolvidos pelo servidor;
- repetir automaticamente o ataque;
- procurar sequências longas de caracteres legíveis;
- exibir o vazamento em formato hexadecimal e textual.

O atacante não conhece antecipadamente:

- a string sensível;
- o conteúdo esperado;
- um marcador como `SEGREDO:`;
- a tentativa em que o dado será encontrado.

### 3. Simulação probabilística

Nas três primeiras tentativas, a string sensível é posicionada fora da região enviada ao atacante.

A partir da quarta tentativa, existe uma probabilidade aproximada de 25% de a informação ficar dentro dos 256 bytes devolvidos pelo servidor.

Isso evita que o segredo seja encontrado obrigatoriamente na primeira execução e representa a possibilidade de uma informação sensível estar próxima do buffer vulnerável dependendo da disposição da memória.

### 4. Análise do vazamento

O atacante não procura uma frase específica. Ele identifica uma possível informação sensível ao encontrar uma sequência com pelo menos 20 caracteres imprimíveis consecutivos.

Quando isso ocorre, são exibidos:

- o número da tentativa;
- a sequência legível descoberta;
- os 256 bytes em hexadecimal;
- a representação textual da memória vazada.

## Requisitos do enunciado

| Requisito | Implementação |
|---|---|
| Criar um processo servidor utilizando socket | Implementado em `src/server.c` |
| Criar um processo atacante | Implementado em `src/attacker.c` |
| Solicitar um buffer grande | O atacante declara 256 bytes |
| Enviar um payload pequeno | O payload `PING` possui 4 bytes |
| Realizar um over-read | O servidor responde usando o tamanho declarado |
| Repetir o ataque | São realizadas até 100 tentativas automaticamente |
| Encontrar uma string por acaso | A posição do segredo varia probabilisticamente |
| Utilizar `malloc()` | A região vulnerável, o segredo e os blocos auxiliares são alocados no heap |
| Realizar várias alocações e desalocações | A função `prepare_heap()` manipula diferentes blocos antes do ataque |
| Não utilizar explicitamente a pilha no ataque | Os buffers principais da demonstração são alocados dinamicamente |
| Trabalho individual | Toda a implementação foi realizada individualmente |

## Como organizei a resolução

O desenvolvimento foi dividido em etapas pequenas:

1. criação do socket do servidor;
2. associação ao endereço local e à porta;
3. aceitação de conexões;
4. criação do cliente atacante;
5. definição do protocolo da mensagem;
6. envio do tamanho declarado e do tamanho real;
7. implementação da resposta vulnerável;
8. visualização dos bytes recebidos;
9. preparação do heap;
10. inclusão da string sensível;
11. repetição automática das tentativas;
12. remoção do conhecimento prévio do segredo pelo atacante;
13. introdução da posição probabilística da informação;
14. criação do `Makefile`;
15. validação final da prova de conceito.

Essa organização permitiu testar cada parte separadamente antes de chegar ao comportamento final.

## Estrutura do repositório

```text
.
├── src/
│   ├── attacker.c
│   └── server.c
├── attacker
├── server
├── .gitignore
├── LICENSE
├── Makefile
└── README.md
```

### Arquivos principais

- `src/server.c`: implementação do servidor vulnerável;
- `src/attacker.c`: implementação do processo atacante;
- `Makefile`: automatização da compilação;
- `server`: executável compilado do servidor;
- `attacker`: executável compilado do atacante.

Os executáveis foram mantidos no repositório como evidência da compilação e da execução realizadas durante o desenvolvimento.

## Como executar

### Pré-requisitos

O projeto foi desenvolvido e testado em ambiente Linux, utilizando:

- GCC;
- GNU Make;
- biblioteca padrão da linguagem C;
- interface de sockets POSIX.

### Compilação

Na raiz do repositório, execute:

```bash
make
```

O comando compila os dois programas:

```text
src/server.c   → server
src/attacker.c → attacker
```

### Execução do servidor

Em um terminal, execute:

```bash
./server
```

O servidor permanecerá aguardando conexões:

```text
Servidor aguardando conexoes em 127.0.0.1:9090...
```

### Execução do atacante

Mantendo o servidor aberto, execute em outro terminal:

```bash
./attacker
```

O atacante realizará novas conexões até encontrar uma sequência legível ou atingir o limite de 100 tentativas.

Exemplo de resultado:

```text
Tentativa 1 de 100...
Nenhuma sequencia relevante encontrada nesta resposta.

Tentativa 2 de 100...
Nenhuma sequencia relevante encontrada nesta resposta.

Tentativa 3 de 100...
Nenhuma sequencia relevante encontrada nesta resposta.

Tentativa 7 de 100...

Possivel dado sensivel encontrado na tentativa 7!
Sequencia legivel descoberta: SEGREDO: ELES ESTAO DE OLHO EM NOS!
```

## Funcionamento do protocolo

Cada solicitação contém:

| Campo | Tamanho | Descrição |
|---|---:|---|
| Tamanho declarado | 2 bytes | Quantidade de bytes solicitada pelo atacante |
| Tamanho real | 2 bytes | Tamanho verdadeiro do payload |
| Payload | Variável | Conteúdo enviado ao servidor |

No ataque executado:

```text
Tamanho declarado: 256 bytes
Tamanho real: 4 bytes
Payload: PING
```

O comportamento correto seria o servidor devolver apenas os 4 bytes do payload.

Entretanto, a implementação vulnerável utiliza o tamanho declarado e devolve 256 bytes, expondo dados que não faziam parte da mensagem original.

## Limitações da demonstração

Esta implementação é uma prova de conceito controlada e não uma reprodução completa do Heartbleed original.

As principais diferenças são:

- não utiliza OpenSSL;
- não implementa TLS;
- não utiliza a extensão Heartbeat;
- limita a comunicação ao endereço local;
- restringe o tamanho máximo das respostas;
- controla probabilisticamente a posição da string sensível;
- utiliza uma região de heap preparada especificamente para tornar o experimento observável e reproduzível.

O objetivo é demonstrar o erro de validação de tamanho e suas consequências, sem criar um servidor vulnerável acessível externamente.

## Conclusão

A prova de conceito demonstra que confiar em um tamanho informado pelo cliente pode causar a exposição de informações que não deveriam fazer parte da resposta.

Embora o atacante envie apenas o payload `PING`, o servidor devolve uma região muito maior da memória porque não compara corretamente o tamanho declarado com o tamanho real da mensagem.

A repetição das solicitações permite que uma informação sensível seja eventualmente encontrada. Além disso, a ausência da string ou de um marcador conhecido no código do atacante demonstra que o conteúdo é obtido a partir dos bytes realmente recebidos do servidor.

A correção fundamental consiste em validar o tamanho declarado e garantir que o servidor nunca envie mais bytes do que aqueles efetivamente recebidos e armazenados para o payload.

> **Uso educacional:** este projeto foi desenvolvido exclusivamente para estudo em ambiente local. O código contém uma vulnerabilidade intencional e não deve ser utilizado em sistemas reais ou disponibilizado em redes externas.