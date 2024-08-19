#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>
#include "../Header/macros.h"
#include "../Header/comunication.h"
#include "../Header/serverfunction.h"

//FUNZIONE CHE CARICA IL MESSAGGIO NELLA GIUSTA FORMA SUL SOCKET
void sender(int client_fd, unsigned int length, char MSG, char* data){
    int retvalue;
    
    //invio della lunghezza
    SYSC(retvalue, write(client_fd, &length, sizeof(length)), "Errore write lunghezzza");
    
    //invio del MSG
    SYSC(retvalue, write(client_fd, &MSG, sizeof(MSG)), "Errore write MSG");
    
    //invio del DATA
    if(length>0){
        SYSC(retvalue, write(client_fd, data, length), "Errore write data");
    }
}

//FUNZIONE CHE PRELEVA E INTERPRETA IL MESSAGGIO DAL SOCKET
char* receiver(int client_fd, char *MSG){
    int length, retvalue;
    char *data = NULL;

    //printf("\nricevo da: %d MSG: %s, data: %s\n\n", client_fd, MSG, data);
    //ricezione della lunghezza
    SYSC(retvalue, read(client_fd, &length, sizeof(length)), "Errore read lunghezzza");

    //ricezione del MSG
    SYSC(retvalue, read(client_fd, MSG, sizeof(char)), "Errore read MSG");
    
    //ricezione del DATA
    if(length >0){
        data = (char*)malloc(length+1);
        if(data == NULL){
            perror("Errore nella malloc");
            return NULL;
        }
        SYSC(retvalue, read(client_fd, data, length), "Errore read data");
        data[length] = '\0';
    }
    return data;
}

//length --> quanti dati distinti ci sono in data
//data --> stringa che contiene la risposta effettiva
