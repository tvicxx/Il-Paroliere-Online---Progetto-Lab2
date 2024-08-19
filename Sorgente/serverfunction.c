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
#include "../Header/macros.h"
#include "../Header/comunication.h"
#include "../Header/serverfunction.h"

//AGGIUNGE UN THREAD ALLA LISTA DI THREAD ATTIVI/CONNSESSIONI ACCETTATE
void add_thread(threadList* lista, pthread_t thread,int client_fd, pthread_mutex_t threadList_mutex){
    thread_node* new = (thread_node*)malloc(sizeof(thread_node));
    new->tid = thread;
    new->fd = client_fd;
    new->next = lista->head;
    lista->head = new;
    lista->count++;
}

//SVUOTA LA LISTA DI THREAD ATTIVI/CONNESSIONI ACCETTATE
void distruggi_threadList(threadList* lista, pthread_mutex_t threadList_mutex){
    pthread_mutex_lock(&threadList_mutex);
    thread_node* temp;
    while(lista->head != NULL){
        temp = lista->head;
        lista->head = lista->head->next;
        free(temp);
    }
    lista->count = 0;
    pthread_mutex_unlock(&threadList_mutex);
}

//RIMUOVE UN THREAD DALLA LISTA DI THREAD ATTIVI/CONNESSIONI ACCETTATE
void rimuovi_thread(threadList* lista, pthread_t thread, pthread_mutex_t threadList_mutex){
    pthread_mutex_lock(&threadList_mutex);
    thread_node *corrente = lista->head;
    thread_node *precedente = NULL;

    while(corrente != NULL){
        if(pthread_equal(corrente->tid, thread) != 0){
            if(precedente == NULL){
                lista->head = corrente->next;
            }
            else{
                precedente->next = corrente->next;
            }
            lista->count--;
            pthread_mutex_unlock(&threadList_mutex);
            return;
        }
        precedente = corrente;
        corrente = corrente->next;
    }
    pthread_mutex_unlock(&threadList_mutex);
}

//CONTROLLA CHE LA STRINGA SIA FATTA SOLO DA NUMERI
int controlloCaratteriNumerici(const char *str){
    while(*str){
        if(!isdigit(*str)){
            return 0; //il carattere non è un numero
        }
        str++;
    }
    return 1;
}

//INIZIALIZZA LA MATRICE IN BASE AI PARAMETRI OPZIONALI PASSATI ALL'APERTURA DEL SERVER
void inizializza_matrice(char* filename, char matrix[][4]){
    static long file_pos = 0; //variabile statica per memorizzare la posizione corrente nel file
    FILE* matrix_file = fopen(filename, "rb+");

    if(matrix_file != NULL){
        fseek(matrix_file, file_pos, SEEK_SET);
        char linea[MAX_BUFFER];
        if(fgets(linea, sizeof(linea), matrix_file) != NULL){ //prende la riga dal file
            char *token = strtok(linea, " ");
            for(int i=0; i<4 && token != NULL; i++){
                for(int j=0; j<4 && token != NULL; j++){
                    if(strcmp(token, "Qu") == 0){
                        matrix[i][j] = 'q';
                    }
                    else{
                        matrix[i][j] = token[0] + 32;
                    }
                    token = strtok(NULL, " ");
                }
            }
            file_pos = ftell(matrix_file);
        }
        else{ 
            //caso in cui ho terminato le matrici disponibili nel file
            file_pos = 0;
            matrix_generator(matrix);
        }
        fclose(matrix_file);
    }
    else{ 
        //caso in cui il file delle matrici non esiste
        matrix_generator(matrix);
    }
}

//GENERA UNA MATRICE DI LETTERE CASUALI
void matrix_generator(char matrix[][4]){
    const char lettere[] = "abcdefghijklmnopqrstuvwxyz";
    for(int i=0; i<4; ++i){
        for(int j=0; j<4; ++j){
            matrix[i][j] = lettere[rand()%26];
        }
    }
}

//INVIA LA MATRICE AL SINGOLO CLIENT
void invio_matrice(int client_fd, char matrix[][4]){
    int length = 16+1;
    char data[length];
    for(int i=0; i<4; i++){
        for(int j=0; j<4; j++){
            data[i * 4 + j] = matrix[i][j];
        }
    }
    data[16] = '\0'; //terminatore della stringa

    //stampa di debug
    printf("Invio matrice al client %d\n", client_fd);
    sender(client_fd, length, MSG_MATRICE, data);
}

//CONTROLLA SE IL NOME DELL'UTENTE CONTIENE SOLO CARATTERI VALIDI O NO
int controllo_caratteri(const char* data){
    while(*data){
        if(!islower(*data) && !isdigit(*data)){
            return 0; //se non contiene lettere minuscole o numeri
        }
        data++;
    }
    return 1;
}

//AGGIUNGE UN GIOCATORE REGISTRATO CORRETTAMENTE ALLA LISTA DI GIOCATORI
void aggiungi_giocatore(lista_giocatori* lista, int client_fd, char* username, pthread_t tid){
    giocatore* new = (giocatore*)malloc(sizeof(giocatore));
    if(new == NULL){
        return;
    }
    new->username = strdup(username);
    if(new->username == NULL){
        return;
    }
    new->tid = tid;
    new->client_fd = client_fd;
    new->next = NULL;

    if(lista->head == NULL){
        lista->head = new;
    }
    else{
        giocatore* corrente = lista->head;
        while(corrente->next != NULL){
            corrente = corrente->next;
        }
        corrente->next = new;
    }
    lista->count++;
    return;
}

//RIMUOVE UN GIOCATORE DALLA LISTA DI GIOCATORI
void rimuovi_giocatore(lista_giocatori* lista, char* username, pthread_mutex_t lista_mutex){
    pthread_mutex_lock(&lista_mutex);
    giocatore *corrente = lista->head;
    giocatore *precedente = NULL;

    while(corrente != NULL){
        if(strcmp(corrente->username, username) == 0){
            if(precedente == NULL){
                lista->head = corrente->next;
            }
            else{
                precedente->next = corrente->next;
            }
            lista->count--;
            pthread_mutex_unlock(&lista_mutex);
            return;
        }
        precedente = corrente;
        corrente = corrente->next;
    }
    pthread_mutex_unlock(&lista_mutex);
    //se il giocatore non è stato trovato, non fa nulla
}

//CONTROLLA SE ESISTE NELLA LISTA GIOCATORI UN GIOCATORE CON LO STESSO USERNAME
int esiste_giocatore(lista_giocatori* lista, char* username, pthread_mutex_t *lista_mutex){
    pthread_mutex_lock(lista_mutex);
    giocatore* corrente = lista->head;
    while(corrente != NULL){
        if(strcmp(corrente->username, username) == 0){
            pthread_mutex_unlock(lista_mutex);
            return 1; //username trovato
        }
        corrente = corrente->next;
    }
    pthread_mutex_unlock(lista_mutex);
    return 0; //username non trovato
}

//FUNZIONE DI DEBUG DEL SERVER CHE STAMPA LA LISTA DI GIOCATORI
void stampa_lista(lista_giocatori* lista, pthread_mutex_t lista_mutex){
    pthread_mutex_lock(&lista_mutex);
    giocatore* corrente = lista->head;
    printf("Lista giocatori di dimensione %d: ", lista->count);
    while(corrente != NULL){
        printf("%s -> ", corrente->username);
        corrente = corrente->next;
    }
    printf("\n");
    pthread_mutex_unlock(&lista_mutex);
    return;
}

//DISTURGGE LA LISTA DI GIOCATORI
void distruggi_lista(lista_giocatori* lista){
    while(lista->head != NULL){
        lista->head = (lista->head)->next;
    }
}

//CALCOLA IL TEMPO RIMANENTE IN SECONDI
char* calcola_tempo_rimanente(time_t start_time, int durata){
    time_t current_time = time(NULL);
    double tempo_trascorso_secondi = difftime(current_time, start_time);
    int tempo_rimanente_secondi = durata - (int)tempo_trascorso_secondi;

    int length = snprintf(NULL, 0, "%d", tempo_rimanente_secondi);
    char* time = (char*)malloc(length+1);
    if(time == NULL){
        perror("Errore nell'allocazione della memoria");
        exit(EXIT_FAILURE);
    }
    snprintf(time, length + 1, "%d", tempo_rimanente_secondi);
    return time;
}

//CREA UN NUOVO NODO DELLA STRUTTURA DATI TRIE
TrieNode* createNode(void){
    TrieNode *newNode = (TrieNode *)malloc(sizeof(TrieNode));
    newNode->boolFineParola = 0;
    for(int i=0; i<26; i++){
        newNode->figli[i] = NULL;
    }
    return newNode;
}

//CARICA IL DIZIONARIO NELLA STRUTTURA DATI TRIE
void caricaDizionario(const char *filename, TrieNode *radice){
    FILE *file = fopen(filename, "r");
    if(file == NULL){
        perror("Errore nell'aprire il file");
        return;
    }
    char bufferParola[100];
    while(fscanf(file, "%99s", bufferParola) != EOF){
        insertTrie(radice, bufferParola);
    }
    fclose(file);
}

//INSERISCE UNA PAROLA NELLA STRUTTURA DATI TRIE
void insertTrie(TrieNode *radice, const char *parola){
    TrieNode *crawler = radice;
    while(*parola){
        int index = *parola - 'a';
        if(!crawler->figli[index]){
            crawler->figli[index] = createNode();
        }
        crawler = crawler->figli[index];
        parola++;
    }
    crawler->boolFineParola = 1;
}

//CERCA UNA PAROLA NELLA STRUTTURA DATI TRIE
int cercaParolaTrie(TrieNode *radice, const char *parola){
    TrieNode *crawler = radice;
    while(*parola){
        int indice = *parola - 'a';
        if(!crawler->figli[indice]){
            return 0;
        }
        crawler = crawler->figli[indice];
        parola++;
    }
    if(crawler != NULL && crawler->boolFineParola){
        return 1;
    }
    else{
        return 0;
    }
}

//FUNZIONE DI DEBUG DEL SERVER CHE STAMPA IL CONTENUTO DELLA STRUTTURA DATI TRIE PER CONTROLLARE SE HA CARICATO CORRETTAMENTE IL DIZIONARIO
void stampaTrie(TrieNode *radice, char *prefisso, int lunghezza){
    if(radice->boolFineParola){
        prefisso[lunghezza] = '\0';
        printf("%s\n", prefisso);
    }

    for(int i=0; i<26; i++){
        if(radice->figli[i]){
            prefisso[lunghezza] = 'a' + i;
            stampaTrie(radice->figli[i], prefisso, lunghezza+1);
        }
    }
}

//AGGIUNGE UNA PAROLA TROVATA ALLA LISTA DI PAROLE TROVATE DA QUEL GIOCATORE DURANTE QUELLA PARTITA
paroleTrovate* aggiungi_parolaTrovata(paroleTrovate* head, const char* parola){
    paroleTrovate* new_node = (paroleTrovate*)malloc(sizeof(paroleTrovate));
    if(!new_node){
        perror("Errore di allocazione della memoria\n");
        return head;
    }
    new_node->parola = strdup(parola);
    if(!new_node->parola){
        perror("Errore di allocazione della memoria per la parola\n");
        return head;
    }
    new_node->next = head;
    return new_node;
}

//CONTROLLA SE UNA PAROLA È GIÀ STATA TROVATA DALL'UTENTE
int esiste_paroleTrovate(paroleTrovate* head, const char* parola){
    if(head == NULL){
        return 0; //lista vuota
    }
    paroleTrovate* current = head;
    while(current != NULL){
        if(strcmp(current->parola, parola) == 0){
            return 1; //parola trovata, quindi già proposta precedentemente
        }
        current = current->next;
    }
    return 0; //parola non trovata, quindi valida
}

//CANCELLA LA LISTA DI PAROLE TROVATE DA UN GIOCATORE, SI USA ALLA FINE DI OGNI PARTITA PER PREPARARSI ALLA PROSSIMA
void cancella_lista_paroleTrovate(paroleTrovate* head){
    paroleTrovate* current = head;
    paroleTrovate* next_node;
    while(current != NULL){
        next_node = current->next;
        current = next_node;
    }
}

//CONTROLLA SE UNA CELLA È VALIDA E NON È GIÀ STATA VISITATA
int isSafe(int x, int y, charMatrix* vector, int index){
    if(x < 0 || x >= 4 || y < 0 || y >= 4){
        return 0;
    }
    for(int i=0; i<index; i++){
        if(vector[i].x == x && vector[i].y == y){
            return 0;
        }
    }
    return 1;
}

//CERCA LA PAROLA NELLA MATRICE IN MODO RICORSIVO PARTENDO DALLA PRIMA LETTERA
int cercaParola(char matrix[][4], charMatrix* vector, int index, int lenParola){
    //se abbiamo trovato tutte le lettere, la parola è stata trovata
    if(index == lenParola){
        return 1;
    }
    int x = vector[index - 1].x;
    int y = vector[index - 1].y;

    //direzioni di esplorazione della matrice: destra, sinistra, giù, su
    int direzioni[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};

    //prova tutte le direzioni possibili
    for(int k=0; k<4; k++){
        int newX = x + direzioni[k][0];
        int newY = y + direzioni[k][1];
        if(isSafe(newX, newY, vector, index)){ //controllo se sono dentro la matrice e se non ho già visitato la cella
            if(matrix[newX][newY] == vector[index].lettera){
                vector[index].x = newX;
                vector[index].y = newY;
                if(cercaParola(matrix, vector, index + 1, lenParola)){
                    return 1; //parola trovata, quindi aggiornare punteggio
                }
                //parola non trovata, quindi provare altre prime lettere nella matrice
                vector[index].x = -1;
                vector[index].y = -1;
            }
        }
        else{
            continue;
        }
    }
    return 0;
}

//CERCA LE LETTERE SUCCESIVE DELLA PAROLA NELLA MATRICE
int cercaParolaMatrice(char matrix[][4], const char* parola){
    // Gestione del carattere speciale "Qu"
    int lenParola = strlen(parola);
    if(strstr(parola, "qu")){
        lenParola--;
    }

    //copia dei caratteri della parola nel vettore
    charMatrix* vector = (charMatrix*)malloc(lenParola * sizeof(charMatrix));
    int j = 0;
    for(int i=0; i<strlen(parola); i++){
        vector[j].lettera = parola[i];
        vector[j].x = -1;
        vector[j].y = -1; 
        if(parola[i] == 'q' && parola[i + 1] == 'u'){
            i++;
        }
        j++;
    }

    for(int i=0; i<4; i++){
        for(int j=0; j<4; j++){
            if(matrix[i][j] == vector[0].lettera){
                vector[0].x = i;
                vector[0].y = j;
                if(cercaParola(matrix, vector, 1, lenParola)){
                    return 1; //parola trovata nella matrice
                }
                vector[0].x = -1;
                vector[0].y = -1;
            }
        }
    }
    return 0; //parola non trovata nella matrice
}

//AGGIORNA IL PUNTEGGIO DI UN GIOCATORE NELLA LISTA DI GIOCATORI
void aggiorna_punteggio(lista_giocatori* lista, char* username, int punteggio){
    giocatore* corrente = lista->head;
    while(corrente != NULL){
        if(strcmp(corrente->username, username) == 0){
            corrente->punteggio = punteggio;
            break;
        }
        corrente = corrente->next;
    }
}

//INVIA UN SEGNALE A TUTTI I GIOCATORI NELLA LISTA DI GIOCATORI
void invia_SIG(lista_giocatori* lista, int SIG, pthread_mutex_t lista_mutex){
    pthread_mutex_lock(&lista_mutex);
    giocatore* corrente = lista->head;
    while(corrente != NULL){
        pthread_kill(corrente->tid, SIG);
        corrente = corrente->next;
    }
    pthread_mutex_unlock(&lista_mutex);
}

//ATTRAVERSO IL TID RISALE ALL'USERNAME DEL GIOCATORE
char* from_tid_to_username(lista_giocatori* lista, pthread_t tid){
    giocatore* corrente = lista->head;
    while(corrente != NULL){
        if(pthread_equal(corrente->tid, tid)){
            return corrente->username;
        }
        corrente = corrente->next;
    }
    return NULL;
}

//ATTRAVERSO IL TID RISALE AL PUNTEGGIO DEL GIOCATORE
int from_tid_to_punteggio(lista_giocatori* lista, pthread_t tid){
    giocatore* corrente = lista->head;
    while(corrente != NULL){
        if(pthread_equal(corrente->tid, tid)){
            return corrente->punteggio;
        }
        corrente = corrente->next;
    }
    return -1;
}

//AGGIUNGE risGiocatore ALLA LISTA DEI RISULTATI DA PROPORRE ALLO SCORER
void pushRisList(risList** head, const char* username, const int punteggio){
    risGiocatore* newGiocatore = (risGiocatore*)malloc(sizeof(risGiocatore));
    if(newGiocatore == NULL){
        perror("Errore di allocazione della memoria\n");
        return;
    }
    newGiocatore->username = strdup(username);
    newGiocatore->punteggio = punteggio;

    risList* newNode = (risList*)malloc(sizeof(risList));
    if(newNode == NULL){
        perror("Errore di allocazione della memoria\n");
        return;
    }
    newNode->ris = newGiocatore;
    newNode->next = NULL;

    if(*head == NULL){
        *head = newNode;
    }
    else{
        risList* current = *head;
        while(current->next != NULL){
            current = current->next;
        }
        current->next = newNode;
    }
}

//ESTRAE risGiocatore DALLA LISTA DEI RISULTATI DA PROPOSTI ALLO SCORER
risGiocatore* popRisList(risList** head){
    if(*head == NULL){
        return NULL;
    }
    risList* temp = *head;
    risGiocatore* ris = temp->ris;
    *head = temp->next;
    return ris;
}
//FUNZIONE COMPARE DEL QSORT
int compare_qsort(const void* a, const void* b){ 
    return ((risGiocatore*)b)->punteggio - ((risGiocatore*)a)->punteggio;
}

//INVIA LA CLASSIFICA FINALE A TUTTI I GIOCATORI
void sendClassifica(lista_giocatori* lista, pthread_t tid, pthread_mutex_t lista_mutex, char* classifica, time_t start_time, int durata_pausa_in_secondi){
    pthread_mutex_lock(&lista_mutex);
    giocatore* corrente = lista->head;
    while(corrente != NULL){
        if(pthread_equal(corrente->tid, tid)){
            sender(corrente->client_fd, strlen(classifica), MSG_PUNTI_FINALI, classifica);
            char *temp = calcola_tempo_rimanente(start_time, durata_pausa_in_secondi);
            sender(corrente->client_fd, strlen(temp), MSG_TEMPO_ATTESA, temp);
        }
        corrente = corrente->next;
    }
    pthread_mutex_unlock(&lista_mutex);
}
