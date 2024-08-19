#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include "../Header/comunication.h"
#include "../Header/serverfunction.h"
#include "../Header/macros.h"

//MUTEX
pthread_mutex_t matrix_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t threadList_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lista_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scorer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pausa_gioco_mutex = PTHREAD_MUTEX_INITIALIZER;

//CONDITION VARIABLES
pthread_cond_t scorer_cond = PTHREAD_COND_INITIALIZER;

//STRUTTURE GLOBALI
char matrix[4][4];

threadList listaThreadAttivi = {NULL, 0};

TrieNode *radice = NULL;

lista_giocatori lista = {NULL, 0};

risList* scoreList = NULL;

char classifica [MAX_BUFFER+MAX_BUFFER];

//VARIABILI GLOBALI
time_t start_time, current_time;
pthread_t scorer_tid, server_tid, game_tid;
int server_fd;

int pausa_gioco = 1;
//pausa_gioco = 0 --> gioco in corso
//pausa_gioco = 1 --> gioco in pausa

int scorerBool = 0;
//scorerBool = 1 --> scorer in esecuzione
//scorerBool = 0 --> scorer in attesa

int classificaBool = 0;
//classificaBool = 1 --> classifica disponibile
//classificaBool = 0 --> classifica non disponibile

//PARAMETRI OPZIONALI
int durata_gioco_in_secondi = 180; //3 minuti
int durata_pausa_in_secondi = 60; //1 minuto
char* dizionario = "dictionary_ita.txt";
char* data_filename = NULL;
int seed = -1;

//STRINGHE DI ERRORE VARIE
char* command_error =   "ATTENZIONE, comando non riconosciuto!\n"
                        "immettere un comando valido\n";

char* word_error =  "ATTENZIONE, gioco in pausa!\n"
                    "non puoi proporre una parola\n";

char* classifica_error = "ATTENZIONE, classifica non disponibile in questo momento!\n";

char* register_error =   "ATTENZIONE, sei già registrato!\n";

char* server_close =    "ATTENZIONE, il server è stato chiuso!\n";

void *scorer(void *arg);

//HANDLER DEI SEGNALI
void alarm_handler(int sig){
    //QUANDO SCADE IL TEMPO SE IL GIOCO ERA IN PAUSA, ALLORA CAMBIA IL BOOL IN MODO CHE IL GIOCO POSSA RICOMINCIARE
    if(pausa_gioco == 1){
        pthread_mutex_lock(&pausa_gioco_mutex);
        pausa_gioco = 0;
        pthread_mutex_unlock(&pausa_gioco_mutex);
    }
    else{
        //QUANDO SCADE IL TEMPO SE IL GIOCO ERA IN CORSO, ALLORA CAMBIA IL BOOL IN MODO CHE IL GIOCO VENGA MESSO IN PAUSA
        int retvalue;
        pthread_mutex_lock(&pausa_gioco_mutex);
        pausa_gioco = 1;
        pthread_mutex_unlock(&pausa_gioco_mutex);
        //INVIO DEL SEGNALE SIGUSR1 A TUTTI I THREAD GIOCATORI NOTIFICANDO CHE IL GIOCO È FINITO
        if(lista.count > 0){
            scorerBool = 1;
            invia_SIG(&lista, SIGUSR1, lista_mutex);
            //CREAZIONE DELLO SCORER
            SYSC(retvalue, pthread_create(&scorer_tid, NULL, scorer, NULL), "Errore nella pthread_create dello scorer");
        }
    }
}

void sigusr1_handler(int sig){
    //QUANDO VIENE RICEVUTO IL SEGNALE SIGUSR1, IL THREAD GIOCATORE CHE LO RICEVE DEVE MANDARE IL PUNTEGGIO E L'USERNAME ALLO SCORER
    pthread_mutex_lock(&lista_mutex);
    //PRENDERE IL PUNTEGGIO E L'USERNAME DALLA LISTA DEI GIOCATORI
    char* username = from_tid_to_username(&lista, pthread_self());
    int punteggio = from_tid_to_punteggio(&lista, pthread_self());
    //INIZIALIZZA IL PUNTEGGIO A 0 NELLA LISTA DEI GIOCATORI
    aggiorna_punteggio(&lista, username, 0);
    pthread_mutex_unlock(&lista_mutex);

    //MANDARE IL PUNTEGGIO E L'USERNAME ALLO SCORER
    pthread_mutex_lock(&scorer_mutex);
    pushRisList(&scoreList, username, punteggio);
    pthread_cond_signal(&scorer_cond);
    pthread_mutex_unlock(&scorer_mutex);
}

void sigusr2_handler(int sig){
    //QUANDO VIENE RICEVUTO IL SEGNALE SIGUSR2, LO SCORER DEVE MANDARE LA CLASSIFICA A TUTTI I THREAD GIOCATORI
    scorerBool = 0;
    sendClassifica(&lista, pthread_self(), lista_mutex, classifica, start_time, durata_pausa_in_secondi);
}

void sigint_handler(int sig){
    //QUANDO RICEVO UN SIGINT DA SERVER
    int retvalue;
    //SCORRO LA LISTA DEI THREAD ATTIVI, COSì DA CHIUDERE TUTTI I THREAD, ANCHE QUELLI NON ANCORA REGISTRATI
    if(listaThreadAttivi.count != 0){
        pthread_mutex_lock(&threadList_mutex);
        thread_node* corrente = listaThreadAttivi.head;
        while(corrente != NULL){
            //MANDO UN MESSAGGIO DI CHIUSURA A TUTTI I THREAD GIOCATORI
            sender(corrente->fd, strlen(server_close), MSG_CHIUSURA_CLIENT, server_close);
            pthread_cancel(corrente->tid);
            corrente = corrente->next;
        }
        pthread_mutex_unlock(&threadList_mutex);
    }
    //DISTRUGGO LE LISTE THREAD E GIOCATORI
    distruggi_lista(&lista);
    distruggi_threadList(&listaThreadAttivi, threadList_mutex);

    printf("\nChiusura del server\n");

    SYSC(retvalue, close(server_fd), "Errore nella chiusura del server_fd");
    exit(EXIT_SUCCESS);
}

//FUNZIONE CHE GESTISCE I THREAD GIOCATORI
void *gestore_thread(void *args){
    int client_fd = *(int *)args;
    int punteggio;
    char* username;
    char MSG;

    //registrazione dell'utente - si esegue fino a quando l'utente non si registra correttamente
    //gli unici comandi validi sono MSG_REGISTRA_UTENTE e (gestito dal server)
    while(1){
        char* data = receiver(client_fd, &MSG);
        if(MSG == MSG_CHIUSURA_CLIENT){
            //CHIUSURA DEL CLIENT NEL CASO DI RICEZIONE DEL MSG_CHIUSURA_CLIENT (CHE PUò ESSERE DOVUTO A SIGINT O A COMANDO fine)
            //non serve eliminare l'utente dalla lista giocatori perchè non è stato ancora aggiunto
            rimuovi_thread(&listaThreadAttivi, pthread_self(), threadList_mutex);
            pthread_exit(NULL);
            return NULL;
        }
        if(data == NULL || MSG != MSG_REGISTRA_UTENTE || strlen(data) > 10 || controllo_caratteri(data) == 0){
            char *str = "Errore, messaggio non valido o nome utente troppo lungo.\nSono validi solo i caratteri minuscoli e/o numerici.\nRiprova\n";
            sender(client_fd, strlen(str), MSG_ERR, str);
            continue;
        }

        //USERNAME VALIDO, ORA CONTROLLI SU LISTA GIOCATORI
        printf("client_fd = %d, username = %s\n", client_fd, data);

        //CONTROLLO SE SONO ARRIVATO AL MASSIMO DI GIOCATORI (32)
        if(lista.count == MAX_CLIENTS){
            char *str = "Numero massimo di giocatori raggiunto. Riprova più tardi\n";
            sender(client_fd, strlen(str), MSG_ERR, str);
            continue;
        }
        //CONTROLLO SE L'USERNAME ESISTE GIÀ NELLA LISTA DEI GIOCATORI
        if(lista.count > 0 && esiste_giocatore(&lista, data, &lista_mutex) == 1){
            printf("client_fd = %d, username già esistente\n", client_fd);
                
            char *str = "Username già esistente. Inserisci un nuovo username: \n";
            sender(client_fd, strlen(str), MSG_ERR, str);
            continue;
        }

        //AGGIUNTA DEL GIOCATORE ALLA LISTA GIOCATORI
        pthread_t tid = pthread_self();
        pthread_mutex_lock(&lista_mutex);
        aggiungi_giocatore(&lista, client_fd, data, tid);
        pthread_mutex_unlock(&lista_mutex);

        username = strdup(data);

        printf("client_fd = %d, username accettato\n", client_fd);

        char msg[MAX_BUFFER];
        snprintf(msg, sizeof(msg), "Benvenuto, %s!\n", username);
        sender(client_fd, strlen(msg), MSG_OK, msg);

        //STAMPA DELLA LISTA SU SERVER PER DEBUG
        stampa_lista(&lista, lista_mutex);
        break;
    }

    //REGISTRAZIONE DEI SEGNALI
    signal(SIGUSR1, sigusr1_handler);

    //registrazione completata, quindi tutti i comandi sono abilitati, ma dipendono dalla fase di gioco in cui ci troviamo
    printf("client_fd = %d, registrazione completata, pausa gioco = %d\n", client_fd, pausa_gioco);

    //INIZIALIZZAZIONE DELLA LISTA PER LE PAROLE TROVATE
    paroleTrovate* listaParoleTrovate = NULL;
    
    //INVIO DELLA MATRICE E DEL TEMPO RIMANENTE IN BASE ALLA FASE DI GIOCO IN CUI SI TROVA IL GIOCATORE DOPO LA REGISTRAZIONE
    if(pausa_gioco == 0){
        printf("Il gioco è in corso\n");
        invio_matrice(client_fd, matrix);
        char *temp = calcola_tempo_rimanente(start_time, durata_gioco_in_secondi);
        sender(client_fd, strlen(temp), MSG_TEMPO_PARTITA, temp);
    }
    else{
        char *temp = calcola_tempo_rimanente(start_time, durata_pausa_in_secondi);
        sender(client_fd, strlen(temp), MSG_TEMPO_ATTESA, temp);
    }

    //CICLO DI GESTIONE DEI COMANDI RICEVUTI DA CLIENT
    while(1){
        while(pausa_gioco == 1 && listaParoleTrovate != NULL){
            cancella_lista_paroleTrovate(listaParoleTrovate);
            punteggio = 0;
        }
        MSG = 0;
        char *data = receiver(client_fd, &MSG);
        switch(MSG){
            case MSG_MATRICE:
                if(pausa_gioco == 0){
                    //GIOCO IN CORSO --> INVIO MATRICE ATTUALE E TEMPO DI GIOCO RIMANENTE
                    invio_matrice(client_fd, matrix);
                    char *temp = calcola_tempo_rimanente(start_time, durata_gioco_in_secondi);
                    sender(client_fd, strlen(temp), MSG_TEMPO_PARTITA, temp);
                }
                else{
                    //PAUSA IN CORSO --> INVIO TEMPO DI PAUSA RIMANENTE
                    char *temp = calcola_tempo_rimanente(start_time, durata_pausa_in_secondi);
                    sender(client_fd, strlen(temp), MSG_TEMPO_ATTESA, temp);
                }
                break;
            
            case MSG_PAROLA:
                if(pausa_gioco == 0){
                    //CONTROLLO DEI CARATTERI DELLA PAROLA INSERITA
                    if(controllo_caratteri(data) == 0){
                        char* caratteriNonValidi = "ATTENZIONE, caratteri non validi!\nUsare solo caratteri alfabetici minuscoli.\n";
                        sender(client_fd, strlen(caratteriNonValidi), MSG_ERR, caratteriNonValidi);
                        break;
                    }
                    //CONTROLLO SE LA PAROLA È GIÀ STATA TROVATA
                    if(esiste_paroleTrovate(listaParoleTrovate, data)){
                        sender(client_fd, 1, MSG_PUNTI_PAROLA, "0");
                        break;
                    }
                    //CONTROLLO SE LA PAROLA È PRESENTE NELLA MATRICE
                    else if(!cercaParolaMatrice(matrix, data)){
                        char* parolaNonTrovataMatrice = "Parola non trovata nella Matrice\n";
                        sender(client_fd, strlen(parolaNonTrovataMatrice), MSG_ERR, parolaNonTrovataMatrice);
                        break;
                    }
                    //CONTROLLO SE LA PAROLA È PRESENTE NEL DIZIONARIO
                    else if(!cercaParolaTrie(radice, data)){
                        char* parolaNonTrovataDiz = "Parola non trovata nel Dizionario\n";
                        sender(client_fd, strlen(parolaNonTrovataDiz), MSG_ERR, parolaNonTrovataDiz);
                        break;
                    }
                    //SE TUTTI I CONTROLLI VANNO A BUON FINE, ALLORA POSSO AGGIUNGERE LA PAROLA ALLA LISTA DELLE PAROLE TROVATE
                    else{
                        listaParoleTrovate = aggiungi_parolaTrovata(listaParoleTrovate, data);
                        int puntiParola = strlen(data);
                        //DECREMENTO DI 1 PUNTO SE LA PAROLA CONTIENE LA LETTERA 'q' E LA LETTERA 'u' DOPO
                        if(strstr(data, "qu")){
                            puntiParola--;
                        }
                        punteggio += puntiParola;

                        //AGGIORNO IL PUNTEGGIOO NELLA LISTA DEI GIOCATORI
                        pthread_mutex_lock(&lista_mutex);
                        aggiorna_punteggio(&lista, username, punteggio);
                        pthread_mutex_unlock(&lista_mutex);
                        
                        //PREPARO LA STRINGA DA MANDARE AL CLIENT CON I PUNTI DELLA PAROLA
                        int length = snprintf(NULL, 0, "%d", puntiParola);
                        char* puntiParola_string = (char*)malloc((length + 1) * sizeof(char));
                        if(!puntiParola_string){
                            perror("Errore nell'allocazione della memoria");
                            exit(EXIT_FAILURE);
                        }
                        snprintf(puntiParola_string, length + 1, "%d", puntiParola);
                        sender(client_fd, strlen(puntiParola_string), MSG_PUNTI_PAROLA, puntiParola_string);
                        
                        //STAMPA DI DEBUG DELLE PAROLE TROVATE SU SERVER
                        printf("[%s] ha trovato la parola [%s], punteggio = %d\n", username, data, punteggio);

                        break;
                    }
                }
                else{
                    //SE IL GIOCO È IN PAUSA, NON POSSO SOTTOMETTERE PAROLE
                    sender(client_fd, strlen(word_error), MSG_ERR, word_error);
                }
                break;

            case MSG_REGISTRA_UTENTE:
                //SE ARRIVA IL MSG_REGISTRA_UTENTE, MANDO ERRORE VISTO CHE IL GIOCATORE È GIÀ REGISTRATO
                sender(client_fd, strlen(register_error), MSG_ERR, register_error);
                break;

            case MSG_PUNTI_FINALI:
                if(pausa_gioco == 1 && classificaBool == 1){
                    sender(client_fd, strlen(classifica), MSG_PUNTI_FINALI, classifica);
                    char *temp = calcola_tempo_rimanente(start_time, durata_pausa_in_secondi);
                    sender(client_fd, strlen(temp), MSG_TEMPO_ATTESA, temp);
                }
                else{
                    sender(client_fd, strlen(classifica_error), MSG_ERR, classifica_error);
                }
                break;
            case MSG_CHIUSURA_CLIENT:
                //CHIUSURA DEL CLIENT NEL CASO DI RICEZIONE DEL MSG_CHIUSURA_CLIENT (CHE PUò ESSERE DOVUTO A SIGINT O A COMANDO fine)
                printf("client_fd = %d, chiusura del client\n", client_fd);
                rimuovi_thread(&listaThreadAttivi, pthread_self(), threadList_mutex);
                rimuovi_giocatore(&lista, username, lista_mutex);
                printf("giocatore [%s] disconnesso\n", username);
                pthread_exit(NULL);
        }
    }
}
//FUNZIONE CHE GESTISCE IL THREAD SCORER
void *scorer(void *arg){
    //SVUOTO LA CLASSIFICA
    memset(classifica,0,strlen(classifica));
    printf("scorer in esecuzione\n");

    //REGISTRAZIONE DEI SEGNALI
    signal(SIGUSR2, sigusr2_handler);

    //PRENDO IL NUMERO DI GIOCATORI REGISTRATI
    pthread_mutex_lock(&lista_mutex);
    int num_giocatori = lista.count;
    pthread_mutex_unlock(&lista_mutex);
    
    //CREO UN VETTORE DI RISULTATI DEI GIOCATORI
    risGiocatore  scoreVector[num_giocatori];

    //PRODUTTORE-CONSUMATORE CHE PRENDE DALLA LISTA DEI RISULTATI MANDATI DAI THREAD GIOCATORI I PUNTEGGI E GLI USERNAME E LI INSERISCE NEL VETTORE
    for(int i=0; i<num_giocatori; i++){
        pthread_mutex_lock(&scorer_mutex);
        while(scoreList == NULL){
            pthread_cond_wait(&scorer_cond, &scorer_mutex);
        }
        risGiocatore* temp = popRisList(&scoreList);
        scoreVector[i].username = temp->username;
        scoreVector[i].punteggio = temp->punteggio;
        pthread_mutex_unlock(&scorer_mutex);
    }
    //SORT DEL VETTORE DEI RISULTATI DEI GIOCATORI
    qsort(scoreVector, num_giocatori, sizeof(risGiocatore), compare_qsort);

    //CREAZIONE DELLA STRINGA DELLA CLASSIFICA IN FORMATO CSV.
    //ESEMPIO PRATICO: "username1,punteggio1,username2,punteggio2,..."
    char msg[MAX_BUFFER];
    for(int i=0; i<num_giocatori; i++){
        snprintf(msg, sizeof(msg), "%s,%d,", scoreVector[i].username, scoreVector[i].punteggio);
        strcat(classifica, msg);
    }
    strcat(classifica, "\0");

    printf("classifica pronta\n");
    classificaBool = 1;
    //INVIO SEGNALE A TUTTI I THREAD GIOCATORI NOTIFICANDOGLI CHE POSSONO PRELEVARE LA CLASSIFICA
    invia_SIG(&lista, SIGUSR2, lista_mutex);
    return NULL;
}

//FUNZIONE CHE GESTISCE IL THREAD DI GIOCO
//GESTISCE SIA IL TEMPO, CHE LA PREPARAZIONE DEI ROUND IN BASE ALLE FASI DI GIOCO
void *game(void *arg){
    int round_pronto = 0;
    while(1){
        if(lista.count == 0){
            printf("Nessun giocatore registrato, attesa...\n");
            //ATTESA FINO A QUANDO NON SI REGISTRA UN NUOVO GIOCATORE
            while(lista.count == 0){
                time(&start_time);
            }
        }
        //REGISTRAZIONE DEI SEGNALI
        signal(SIGALRM, alarm_handler);
        
        //INIZIA LA PAUSA
        time(&start_time);
        alarm(durata_pausa_in_secondi);
        printf("-----------------------------------------------------------------------\n");
        printf("la partita è finita, siamo in pausa, orario inizio: %ld\n", start_time);

        //SE IL ROUND NON è STATO PREPARATO ALLORA LO PREPARA
        if(round_pronto == 0){
            pthread_mutex_lock(&matrix_mutex);
            inizializza_matrice(data_filename, matrix);
            pthread_mutex_unlock(&matrix_mutex);
            round_pronto = 1;
        }

        while(pausa_gioco){
            //attesa
        }
        //FINE ATTESA

        //SE ALLA FINE DELLA PAUSA NON CI SONO GIOCATORI REGISTRATI, SI RIPETE IL CICLO DA CAPO E SI ATTENDONO NUOVI GIOCATPORI, MANTENENDO IL ROUND GIÀ PREPARATO
        if(lista.count == 0){
            pausa_gioco = 1;
            continue;
        }

        //INIZIA IL GIOCO
        time(&start_time);
        alarm(durata_gioco_in_secondi);
        classificaBool = 0;
        printf("-----------------------------------------------------------------------\n");
        printf("la partita è iniziata alle %ld con %d giocatori\n", start_time, lista.count);
        
        //INVIO DELLA MATRICE E DEL TEMPO RIMANENTE AI GIOCATORI REGISTRATI E QUINDI PARTECIPANTI AL GIOCO
        pthread_mutex_lock(&lista_mutex);
        giocatore* current = lista.head;
        while (current != NULL){
            invio_matrice(current->client_fd, matrix);
            char *temp = calcola_tempo_rimanente(start_time, durata_gioco_in_secondi);
            sender(current->client_fd, strlen(temp), MSG_TEMPO_PARTITA, temp);
            current = current->next;
        }
        pthread_mutex_unlock(&lista_mutex);
        
        while(!pausa_gioco){
            //attesa
        }
        //FINE GIOCO

        //NOTIFICA CHE IL ROUND PREPARATO È STATO UTILIZZATO E QUINDI BISOGNA PREPARARNE UNO NUOVO
        round_pronto = 0;
    }
}

//FUNZIONE CHE GESTISCE IL SERVER
void server(char* nome_server, int porta_server){
    int client_fd, retvalue;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    //REGISTRAZIONE DEI SEGNALI
    signal(SIGINT, sigint_handler);

    //CREAZIONE DEL SOCKET
    SYSC(server_fd, socket(AF_INET, SOCK_STREAM, 0), "Errore nella socket");

    //inizializzazione nella struttura server_addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(porta_server);
    server_addr.sin_addr.s_addr = inet_addr(nome_server);

    //binding
    SYSC(retvalue, bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)), "Errore nella bind");

    //listen
    SYSC(retvalue, listen(server_fd, MAX_CLIENTS), "Errore nella listen");

    printf("STO ASCOLTANDO\n");

    //thread del gioco
    SYSC(retvalue, pthread_create(&game_tid, NULL, game, NULL), "Errore nella pthread_create del gioco");

    while(1){
        //accept
        SYSC(client_fd, accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len), "Errore nella accept");
        pthread_t thread;
        SYSC(retvalue, pthread_create(&thread, NULL, gestore_thread, &client_fd), "Errore nella pthread_create");

        //AGGIUNGO IL THREAD APPENA CREATO ALLA LISTA DEI THREAD ATTIVI
        add_thread(&listaThreadAttivi, thread, client_fd, threadList_mutex);
    }
}

//MAIN CHE GESTISCE I PARAMETRI OBBLIGATORI E OPZIONALI DA STDIN
int main(int argc, char * ARGV[]){
    //controllo che i parametri obbligatori siano presenti
    if(argc < 3 || controlloCaratteriNumerici(ARGV[2]) == 0){
        printf("Errore nella sintassi di %s\n", ARGV[0]);
        exit(EXIT_FAILURE);
    }
    
    //parametri obbligatori
    char* nome_server = ARGV[1];
    int porta_server = atoi(ARGV[2]);

    int retvalue;
    
    //controllo dei parametri opzionali da stdin

    //variabili per la gestione della libreria getopt
    int opt;
    int option_index = 0;
    
    //definizione delle opzioni lunghe per semplificarne il riconoscimento durante lo switch
    static struct option long_options[] = {
        {"matrici", required_argument, 0, 'm'},
        {"durata", required_argument, 0, 'd'},
        {"seed", required_argument, 0, 's'},
        {"diz", required_argument, 0, 'z'},
        {0,0,0,0}
    };

    while((opt = getopt_long(argc, ARGV, "", long_options, &option_index)) != -1){
        switch(opt){
            case 'm':
                data_filename = optarg;
                break;
            
            case 'd':
                durata_gioco_in_secondi = atoi(optarg)*60;
                break;
            
            case 's':
                seed = atoi(optarg);
                break;
            
            case 'z': 
                dizionario = optarg;
                break;

            default:
                printf("Errore nella sintassi di %s\n", ARGV[0]);
                exit(EXIT_FAILURE);
        }
    }

    //CREAZIONE DEL TRIE E CARICAMENTO DEL DIZIONARIO
    radice = createNode();
    caricaDizionario(dizionario, radice);

    srand(seed);
    /*
    //FUNZIONE DI DEBUG PER STAMPARE IL DIZIONARIO MEMORIZZATO NEL TRIE
    char prefisso[100];
    stampaTrie(radice, prefisso, 0);
    */

    //STAMPA DEI PARAMETRI DI GIOCO
    printf("\nIMPOSTAZIONI DI GIOCO\n");
    printf("matrice: %s\n", data_filename);
    printf("durata: %d\n", durata_gioco_in_secondi);
    printf("seed: %d\n", seed);
    printf("dizionario: %s\n", dizionario);
    printf("------------------------------------------\n");

    server_tid = pthread_self();

    //CREAZIONE DEL SERVER
    server(nome_server, porta_server);

    SYSC(retvalue, wait(NULL), "Errore nella wait");

    return 0;
}
