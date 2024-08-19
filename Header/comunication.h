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

void sender(int client_fd, unsigned int len, char MSG, char* data);

char* receiver(int client_fd, char* MSG);
