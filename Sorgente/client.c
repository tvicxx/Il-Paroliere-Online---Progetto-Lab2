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
#include <signal.h>
#include <sys/wait.h>
#include "../Header/comunication.h"
#include "../Header/macros.h"  


//STRINGHE DI MESSAGGIO
char* hello =
    "\n"
    "██████╗  █████╗ ██████╗  ██████╗ ██╗     ██╗███████╗██████╗ ███████╗\n"
    "██╔══██╗██╔══██╗██╔══██╗██╔═══██╗██║     ██║██╔════╝██╔══██╗██╔════╝\n"
    "██████╔╝███████║██████╔╝██║   ██║██║     ██║█████╗  ██████╔╝█████╗\n"
    "██╔═══╝ ██╔══██║██╔══██╗██║   ██║██║     ██║██╔══╝  ██╔══██╗██╔══╝\n"
    "██║     ██║  ██║██║  ██║╚██████╔╝███████╗██║███████╗██║  ██║███████╗\n"
    "╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚══════╝╚═╝╚══════╝╚═╝  ╚═╝╚══════╝\n"
    "\n"
    "-----------------------------------------------------------------------\n";

char* help_message =    "\n-----------------------------------------------------------------------\n\nCOMANDI\n"
                        "aiuto --> mostra i comandi disponibili\n"
                        "registra_utente nome_utente --> registrare un nuovo utente\n"
                        "matrice --> stampa a video la matrice corrente e il tempo rimanente\n"
                        "p parola_indicata --> proporre una parola durante il gioco\n"
                        "fine --> uscire dal gioco \n"
                        "\n-----------------------------------------------------------------------\n";



char* close_message =   "\n"
                        "-----------------------------------------------------------------------\n"
                        "Grazie per aver giocato al PAROLIERE\n"
                        "A presto :-)\n"
                        "\n"
                        "Credits: Vicarelli Tommaso - Mat. 638912\n";

char* command_error =   "ATTENZIONE, comando non riconosciuto!\n"
                        "immettere un comando valido\n";

char* prompt =  "\n"
                "[PROMPT PAROLIERE]--> ";

//VARIABILI GLOBALI
int client_fd;
pthread_t main_tid;
pthread_t sender_tid, receiver_tid;

//merchant --> sender
//bouncer --> receiver

volatile int run = 1;

void sigint_handler(int sig){
    int retvalue;
    //INVIO UN SEGNALE AL SENDER DICENDOGLI DI PREPARARSI ALLA CHIUSURA
    pthread_kill(sender_tid,SIGUSR1);
    //CHIUDO IL THREAD RECEIVER PERCHÈ NON ASPETTO PIÙ NULLA DAL SERVER
    pthread_cancel(receiver_tid);
    //ASPETTO LA TERMINAZIONE DELLA GESTIONE DEL SEGNALE DA PARTE DEL SENDER
    pthread_join(sender_tid,NULL);
    //CHIUDO IL SOCKET
    SYSC(retvalue,close(client_fd),"chiusura del client");
    //TERMINO IL CLIENT
    exit(EXIT_SUCCESS);
}

void sigusr1_handler(int sig){
    //QUANDO VIENE RICHIESTA LA CHIUSURA DEL CLIENT TRAMITE SIGINT
    int retvalue;
    //STAMPO ALL'UTENTE IL MESSAGGIO DI CHIUSURA
    SYSC(retvalue, write(STDOUT_FILENO, close_message, strlen(close_message)), "Errore nella write");
    if(pthread_self() == sender_tid){
        //COMUNICO AL SERVER CHE STO TERMINANDO IL CLIENT
        sender(client_fd, 0, MSG_CHIUSURA_CLIENT, NULL);
    }
    //TERMINO L'ESECUZIONE
    pthread_exit(NULL);
    return;
}

//FUNZIONE CHE GESTISCE IL THREAD CHE SI OCCUPA DI INVIARE I MESSAGGI AL SERVER
void* client_sender(void* args){
    client_fd = *((int *)args);

    //legge da stdin
    int retvalue;
    while(run){
        char buffer[MAX_BUFFER];
        ssize_t n_read;
        SYSC(n_read, read(STDIN_FILENO, buffer, MAX_BUFFER), "Errore nella read");

        if(strcmp(buffer, "aiuto\n") == 0){
            //AIUTO VIENE GESTITO SENZA INVIARE NESSUNA RICHIESTA AL SEVER
            SYSC(retvalue, write(STDOUT_FILENO, help_message, strlen(help_message)), "Errore nella write");
            SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
            continue;
        }
        else if(strncmp(buffer, "registra_utente", 15) == 0){
            //REGISTRA_UTENTE MANDA UNA RICHIESTA DI REGISTRAZIONE AL SERVER
            char *data = strtok(buffer, " ");
            data = strtok(NULL, " ");
            if(data == NULL){
                //SE NON VIENE INSERITO UN NOME UTENTE
                SYSC(retvalue, write(STDOUT_FILENO, command_error, strlen(command_error)), "Errore nella write");
                SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
                continue;
            }
            data[strcspn(data, "\n")] = 0;
            sender(client_fd, strlen(data), MSG_REGISTRA_UTENTE, data);
        }
        else if(strcmp(buffer, "matrice\n") == 0){
            //MATRICE MANDA UNA RICHIESTA AL SERVER PER RICEVERE LA MATRICE E/O IL TEMPO IN BASE ALLA FASE DI GIOCO
            sender(client_fd, 0, MSG_MATRICE, NULL);
        }
        else if(strncmp(buffer, "p", 1) == 0){
            //P MANDA UNA RICHIESTA AL SERVER PER PROPORRE UNA PAROLA
            char *data = strtok(buffer, " ");
            data = strtok(NULL, " ");
            if(data == NULL){
                //SE NON VIENE INSERITA UNA PAROLA
                SYSC(retvalue, write(STDOUT_FILENO, command_error, strlen(command_error)), "Errore nella write");
                SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
                continue;
            }
            data[strcspn(data, "\n")] = 0;
            if(strlen(data) < 4){
                //SE LA PAROLA INSERITA È TROPPO CORTA
                SYSC(retvalue, write(STDOUT_FILENO, command_error, strlen(command_error)), "Errore nella write");
                SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
            }
            else{
                //MANDO LA PAROLA AL SERVER
                sender(client_fd, strlen(data), MSG_PAROLA, data);
            }
        }
        else if(strcmp(buffer, "classifica\n") == 0){
            //CLASSIFICA MANDA UNA RICHIESTA AL SERVER PER RICEVERE LA CLASSIFICA FINALE
            sender(client_fd, 0, MSG_PUNTI_FINALI, NULL);
        }
        else if(strcmp(buffer, "fine\n") == 0){
            //FINE MANDA UNA RICHIESTA AL SERVER PER CHIUDERE IL CLIENT
            sender(client_fd, 0, MSG_CHIUSURA_CLIENT, NULL);
            run = 0;
            break;
        }
        else{
            //SE IL COMANDO NON È RICONOSCIUTO
            SYSC(retvalue, write(STDOUT_FILENO, command_error, strlen(command_error)), "Errore nella write");
            SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
        }
        memset(buffer, 0, MAX_BUFFER);
    }
    //STAMPO IL MESSAGGIO DI CHIUSURA E CHIUDO IN CASO DI INSERIMENTO DEL COMANDO fine
    SYSC(retvalue, write(STDOUT_FILENO, close_message, strlen(close_message)), "Errore nella write");
    pthread_cancel(receiver_tid);
    return NULL;
}

//FUNZIONE CHE GESTISCE IL THREAD CHE SI OCCUPA DI RICEVERE I MESSAGGI DAL SERVER
void* client_receiver(void* args){
    client_fd = *((int *)args);
    int retvalue;
    char MSG;

    //riceve dal server
    while(run){
        char* data = receiver(client_fd, &MSG);
        if(data == NULL){
            //SE NON VIENE RICEVUTO NULLA DAL SERVER
            continue;
        }
        switch(MSG){
            case MSG_MATRICE:
                //STAMPA LA MATRICE RICEVUTA DAL SERVER
                printf("\n\n+----+----+----+----+\n");
                for(int i = 0; i < 4; i++){
                    for(int j = 0; j < 4; j++){
                        printf("| ");
                        if(data[i * 4 + j] == 'q'){
                            printf("Qu ");
                        }
                        else{
                            printf("%c  ", data[i * 4 + j]-32);
                        }
                    }
                    printf("|\n+----+----+----+----+\n");
                }
                break;

            case MSG_OK:
                printf("\n\n%s\n", data);

                break;
            
            case MSG_ERR:
                printf("\n\n%s\n", data);
                break;

            case MSG_PUNTI_PAROLA:
                printf("\n\nParola accettata, punti ottenuti: %s\n", data);
                break;

            case MSG_TEMPO_PARTITA:
                //STAMPA IL TEMPO RIMANENTE DELLA PARTITA
                printf("\n\nTempo rimanente: %s sec\n", data);
                break;

            case MSG_TEMPO_ATTESA:
                //STAMPA IL TEMPO DI ATTESA ALLA PROSSIMA PARTITA
                printf("\n\nTempo di attesa alla prossima partita: %s sec\n", data);
                break;

            case MSG_PUNTI_FINALI: {
                //STAMPA LA CLASSIFICA FINALE
                if(strchr(data, ',') == NULL){
                    printf("\n\n%s\n", data);
                    break;
                }
                char *token = strtok(data, ",");
                int posizione = 1;
                printf("\n-----------------------------------------------------------------------\n\nClassifica:\n");
                while(token != NULL){
                    printf("%d. %s: ", posizione, token); //username

                    token = strtok(NULL, ",");
                    printf("%s\n", token); //punteggio

                    token = strtok(NULL, ",");
                    posizione++;
                }
                printf("\n-----------------------------------------------------------------------\n");
                break;
            }

            case MSG_CHIUSURA_CLIENT:
                //SE RICEVE IL MESSAGGIO DI CHIUSURA DAL SERVER
                printf("\n\n%s\n", data);
                SYSC(retvalue, close(client_fd), "Errore nella close");
                exit(EXIT_SUCCESS);
            
            default:
                //SE IL MESSAGGIO RICEVUTO NON È RICONOSCIUTO
                SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
                break;
        }
        SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");
    }
    return NULL;
}

//FUNZIONE MAIN CHE GESTISCE L'AVVIO DEL CLIENT E LA CREAZIONE DEI THREAD SENDER E RECEIVER
int main(int argc, char * ARGV[]){
    //controllo che i parametri obbligatori siano presenti  
    if(argc != 3){
        printf("Errore nella sintassi di %s\n", ARGV[0]);
        exit(EXIT_FAILURE);
    }
    
    char* nome_server = ARGV[1];
    int porta_server = atoi(ARGV[2]);

    int client_fd, retvalue;
    struct sockaddr_in server_addr;

    //creazione del socket
    SYSC(client_fd, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket");

    //iniziallizazione della struttura 
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta_server);
    server_addr.sin_addr.s_addr = inet_addr(nome_server);

    //connect
    SYSC(retvalue, connect(client_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)), "Errore nella connect");

    //scrive all'utente di registrarsi con la sintassi registra_utente nome_utente
    //printf("sto scrivendo da %d\n", client_fd);
    SYSC(retvalue, write(STDOUT_FILENO, hello, strlen(hello)), "Errore nella write");
    char* msg_registrazione = "per registrati -> registra_utente nome_utente\nper conoscere i comandi -> aiuto\n\n";
    SYSC(retvalue, write(STDOUT_FILENO, msg_registrazione, strlen(msg_registrazione)), "Errore nella write");
    SYSC(retvalue, write(STDOUT_FILENO, prompt, strlen(prompt)), "Errore nella write");

    //gestione dei segnali
    signal(SIGINT, sigint_handler);
    signal(SIGUSR1, sigusr1_handler);

    //creazione dei thread
    SYSC(retvalue, pthread_create(&receiver_tid, NULL, client_receiver, &client_fd), "Errore nella create del client_receiver");
    SYSC(retvalue, pthread_create(&sender_tid, NULL, client_sender, &client_fd), "Errore nella create del client_sender");

    //attesa della terminazione dei thread
    SYSC(retvalue, pthread_join(receiver_tid, NULL), "Errore nella join del client_receiver");
    SYSC(retvalue, pthread_join(sender_tid, NULL), "Errore nella join del client_sender")

    return 0;
}
