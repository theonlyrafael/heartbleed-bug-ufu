CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O0 -g

SERVER = server
ATTACKER = attacker

.PHONY: all

all: $(SERVER) $(ATTACKER)

$(SERVER): src/server.c
	$(CC) $(CFLAGS) src/server.c -o $(SERVER)

$(ATTACKER): src/attacker.c
	$(CC) $(CFLAGS) src/attacker.c -o $(ATTACKER)