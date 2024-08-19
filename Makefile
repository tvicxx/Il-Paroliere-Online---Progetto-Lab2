# COMPILATORE
CC = gcc

# OPZIONI DI COMPILAZIONE
CFLAGS = -Wall -g -pedantic - lrt -pthread
#aggiungere -lrt dopo pedantic quando consegno il progetto per compilare su linux

# DIRECTORY
HEADER = ./Header/
SORGENTE = ./Sorgente/

# HEADERS
HDRS1 = $(HEADER)macros.h
HDRS2 = $(HEADER)comunication.h
HDRS3 = $(HEADER)serverfunction.h

# SORGENTI
SERVER = server
CLIENT = client
COMUNICATION = comunication
SERVERFUNCTION = serverfunction

# FILE OGGETTO
OBJS = $(SERVER).o
OBJC = $(CLIENT).o
OBJCOM = $(COMUNICATION).o
OBJSF = $(SERVERFUNCTION).o
OBJECT_FILES = $(OBJC) $(OBJS) $(OBJCOM) $(OBJSF)

# FILE SORGENTE
SRCS = $(SORGENTE)$(SERVER).c
SRCC = $(SORGENTE)$(CLIENT).c
SRCCOM = $(SORGENTE)$(COMUNICATION).c
SRCSF = $(SORGENTE)$(SERVERFUNCTION).c

# INPUT
PORT = 1025
PORT1 = 1026
HOST = 127.0.0.1

# TEST
SERVER_RUN = ./$(SERVER) $(HOST) $(PORT)
SERVER_RUN1 = ./$(SERVER) $(HOST) $(PORT) --matrici ./matrix.txt
SERVER_RUN2 = ./$(SERVER) $(HOST) $(PORT1) --matrici ./matrix.txt --durata 4 --seed 20
CLIENT_RUN = ./$(CLIENT) $(HOST) $(PORT)
CLIENT_RUN1 = ./$(CLIENT) $(HOST) $(PORT1)

# TARGET
all: $(SERVER) $(CLIENT)

# DIPENDENZE ESEGUIBILI
$(SERVER): $(OBJS) $(OBJCOM) $(OBJSF)
	@echo creazione server
	@$(CC) $(CFLAGS) $(OBJS) $(OBJCOM) $(OBJSF) -o $(SERVER)

$(CLIENT): $(OBJC) $(OBJCOM)
	@echo creazione client
	@$(CC) $(CFLAGS) $(OBJC) $(OBJCOM) -o $(CLIENT)

# DIPENDENZE FILE OGGETTO
# server.o
$(OBJS): $(SRCS) $(HDRS1) $(HDRS2) $(HDRS3)
	@echo creazione file oggetto server
	@$(CC) $(CFLAGS) -c $(SRCS) -o $(OBJS)

# client.o
$(OBJC): $(SRCC) $(HDRS1) $(HDRS2)
	@echo creazione file oggetto client
	@$(CC) $(CFLAGS) -c $(SRCC) -o $(OBJC)

# comunication.o
$(OBJCOM): $(SRCCOM) $(HDRS2)
	@echo creazione file oggetto comunication
	@$(CC) $(CFLAGS) -c $(SRCCOM) -o $(OBJCOM)

# serverfunction.o
$(OBJSF): $(SRCSF) $(HDRS3) $(HDRS2) 
	@echo creazione file oggetto serverfunction
	@$(CC) $(CFLAGS) -c $(SRCSF) -o $(OBJSF)

obj_clean:
	@rm -f $(OBJECT_FILES)
	@echo file oggetto rimossi

clean: 
	@rm -f $(OBJECT_FILES) $(SERVER) $(CLIENT)

.PHONY: all obj_clean clean run-server run-server1 run-server2 test-client test-client2

server1025:
	@echo server port $(PORT)
	@$(SERVER_RUN)

server1025M:
	@echo server port $(PORT) con matrix
	@$(SERVER_RUN1)

server1026:
	@echo server port $(PORT1) con matrix, durata e seed
	@$(SERVER_RUN2)

client1025:
	@echo client port $(PORT)
	@$(CLIENT_RUN)

client1026:
	@echo client port $(PORT1)
	@$(CLIENT_RUN1)
