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
#include <time.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#define MSG_OK 'K'
#define MSG_ERR 'E'
#define MSG_REGISTRA_UTENTE 'R'
#define MSG_MATRICE 'M'
#define MSG_TEMPO_PARTITA 'T'
#define MSG_TEMPO_ATTESA 'A'
#define MSG_PAROLA 'W'
#define MSG_PUNTI_FINALI 'F'
#define MSG_PUNTI_PAROLA 'P'

//MESSAGGIO PERSONALIZZATO PER LA CHIUSURA DEL CLIENT
#define MSG_CHIUSURA_CLIENT 'C'

#define MAX_BUFFER 1024
#define MAX_CLIENTS 32

//STRUTTURA DEL SINGOLO GIOCATORE REGISTRATO CORRETTAMENTE
typedef struct giocatore {
    char* username;
    pthread_t tid;
    int client_fd;
    int punteggio;
    struct giocatore* next;
}giocatore;

//LISTA DEI GIOCATORI REGISTRATI CORRETTAMENTE
typedef struct{
    giocatore* head;
    int count;
}lista_giocatori;

//STRUTTURA DEL SINGOLO THREAD ATTIVO
typedef struct thread_node{
    pthread_t tid;
    int fd;
    struct thread_node* next;
}thread_node;

//LISTA DEI THREAD ATTIVI
typedef struct {
    thread_node* head;
    int count;
}threadList;

//LISTA DELLE PAROLE TROVATE
typedef struct paroleTrovate{
    char* parola;
    struct paroleTrovate* next;
}paroleTrovate;

//STRUTTURA DEL NODO DEL TRIE
typedef struct TrieNode {
    struct TrieNode *figli[26];
    int boolFineParola;
}TrieNode;

//STRUUTURA DI APPOGGIO PER CONTROLLARE L'APPARTENENZA DELLA PAROLA ALLA MATRICE
typedef struct charMatrix{
    char lettera;
    int x;
    int y;
}charMatrix;

//STRUTTURA DEL RISULTATO DEL SINGOLO GIOCATORE
typedef struct risGiocatore{
    char* username;
    int punteggio;
}risGiocatore;

//LISTA DI RISULTATI DEI GIOCATORI
typedef struct risList{
    risGiocatore* ris;
    struct risList* next;
}risList;

void add_thread(threadList* lista, pthread_t thread, int client_fd, pthread_mutex_t threadList_mutex);

void distruggi_threadList(threadList* lista, pthread_mutex_t threadList_mutex);

void rimuovi_thread(threadList* lista, pthread_t thread, pthread_mutex_t threadList_mutex);

int controlloCaratteriNumerici(const char *str);

void inizializza_matrice(char* filename, char matrix[][4]);

void matrix_generator(char matrix[][4]);

void invio_matrice(int client_fd, char matrix[][4]);

int controllo_caratteri(const char* data);

void aggiungi_giocatore(lista_giocatori* lista, int client_fd, char* username, pthread_t tid);

void rimuovi_giocatore(lista_giocatori* lista, char* username, pthread_mutex_t lista_mutex);

int esiste_giocatore(lista_giocatori* lista, char* username, pthread_mutex_t *lista_mutex);

void stampa_lista(lista_giocatori* lista, pthread_mutex_t lista_mutex);

void distruggi_lista(lista_giocatori* lista);

char* calcola_tempo_rimanente(time_t start_time, int durata);

TrieNode* createNode(void);

void caricaDizionario(const char *filename, TrieNode *radice);

void insertTrie(TrieNode *radice, const char *parola);

int cercaParolaTrie(TrieNode *radice, const char *parola);

void stampaTrie(TrieNode *radice, char *prefisso, int lunghezza);

paroleTrovate* aggiungi_parolaTrovata(paroleTrovate* head, const char* parola);

int esiste_paroleTrovate(paroleTrovate* head, const char* parola);

void cancella_lista_paroleTrovate(paroleTrovate* head);

int isSafe(int x, int y, charMatrix* vector, int index);

int cercaParola(char matrix[][4], charMatrix* vector, int index, int lenParola);

int cercaParolaMatrice(char matrix[][4], const char* parola);

void aggiorna_punteggio(lista_giocatori* lista, char* username, int punteggio);

void invia_SIG(lista_giocatori* lista, int SIG, pthread_mutex_t lista_mutex);

char* from_tid_to_username(lista_giocatori* lista, pthread_t tid);

int from_tid_to_punteggio(lista_giocatori* lista, pthread_t tid);

void pushRisList(risList** head, const char* username, const int punteggio);

risGiocatore* popRisList(risList** head);

int compare_qsort(const void* a, const void* b);

void sendClassifica(lista_giocatori* lista, pthread_t tid, pthread_mutex_t lista_mutex, char* classifica, time_t start_time, int durata_gioco_in_secondi);
