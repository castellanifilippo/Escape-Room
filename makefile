#compilatore

CC = gcc



# Flags del compilatore

CFLAGS = -Wall



# File oggetto per il client e il server

CLIENT_OBJS = client.o gameClient.o

SERVER_OBJS = server.o gameServer.o



# Eseguibili da creare

all: client server other clear



# Regola per creare l'eseguibile del client

client: $(CLIENT_OBJS)

	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o client

other: $(CLIENT_OBJS)

	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o other


# Regola per creare l'eseguibile del server

server: $(SERVER_OBJS)

	$(CC) $(CFLAGS) $(SERVER_OBJS) -o server -lrt -lpthread


# Regola per compilare client.o

client.o: client.c gameClient.h

	$(CC) $(CFLAGS) -c client.c -o client.o



# Regola per compilare gameClient.o

gameClient.o: gameClient.c gameClient.h

	$(CC) $(CFLAGS) -c gameClient.c -o gameClient.o



# Regola per compilare server.o

server.o: server.c gameServer.h

	$(CC) $(CFLAGS) -c server.c -o server.o



# Regola per compilare gameServer.o

gameServer.o: gameServer.c gameServer.h

	$(CC) $(CFLAGS) -c gameServer.c -o gameServer.o

clear:
	rm -f *.o

