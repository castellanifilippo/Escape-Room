#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gameClient.h"

int sd, ret;    //socket e ritorno
char buffer[BUFFER_SIZE];
int logged = 0; //utente loggato o meno
int connected = 0;  //se ho ricevuto dati per connessione
struct sockaddr_in srv_addr;    //indirizzo server
char usr[MAX_BUFFER];   //nome utente

//dichiarazione funzione
void command();

int main(int argc, char* argv[]) {
    int port;

    //verifico se ho la porta
    if(argc < 2){
        printf("\33[2K\rInserisci una porta\n");
        exit(1);
    }else{
        if (sscanf(argv[1], "%d", &port) != 1){
            printf("\33[2K\rParametro non valido\n");
            exit(1);
        }
    }

    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    /* Creazione indirizzo del server */
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(4242); //porta nel codice come da specifica
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    /* Connessione */
    ret = connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if (ret < 0) {
        perror("Errore in fase di connessione: ");
        exit(1);
    }

    printf("\33[2K\r***************************************************************\n");
    printf("1) login <username> <password> --> collegati con il tuo account\n");
    printf("2) sign <username> <password> --> crea un nuovo account\n");
    printf("***************************************************************\n>");  

    //ciclo finche non si logga
    while(!logged){
        fgets(buffer, BUFFER_SIZE, stdin);
        command();
    }

    printf("\33[2K\r***************************************************************\n");
    printf("1) list --> lista server disponibili\n");
    printf("2) connect <id>--> connettiti al server scelto\n");
    printf("***************************************************************\n>");  

    //ciclo finche non ho info per connessione
    while(!connected){
        fgets(buffer, BUFFER_SIZE, stdin);
        command();
    }

    //mi connetto al server di gioco
    //chiudo socket
    close(sd);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    ret = connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));
    if (ret < 0) {
        perror("\33[2K\rErrore in fase di connessione al server di gioco: ");
        exit(1);
    }
    printf("\33[2K\rTi sei connesso al server di gioco\n>");
    fflush(stdout);
    //passo al gioco
    gameClient();
	return 0;
}

//funzione che gestisce i comandi da tastiera
void command(){
    char *token;    //variabile per divisione input
    char *args[3];  //input diviso
    int argc = 0;   //numero input
    struct Response res; //risposta
    
    //ignoro input \n
    if(strcmp(buffer,"\n") == 0){
        return;
    }
    
    //tolgo \n
    buffer[strlen(buffer) -1] = '\0';

    // divido la stringa
    token = strtok(buffer, " ");
    while (token != NULL && argc < 3) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }

    //controllo e gestisco input
    if(strcmp(args[0], "login") == 0){
        if(logged){
            printf("\33[2K\rComando non valido\n>");
            return;
        }
        if(argc < 3){
            printf("\33[2K\rlogin <username> <password>\n>");
            return;
        }
        //salvo username
        strcpy(usr, args[1]);
        prepare_request(LOGIN, args[1], args[2], buffer, usr);

        ret = send(sd, buffer, sizeof(struct Request), 0);
        if (ret < 0) {
            perror("Errore in fase di login: ");
            exit(1);
        }
        ret = recv(sd, buffer, sizeof(struct Response), 0);
        if (ret < 0) {
            perror("Errore in fase di login: ");
            exit(1);
        }
        convert_response(&res, buffer);
        if(res.status == LOG_OK){
            printf("\33[2K\rLogin effettuato con successo!\n>");
            logged = 1;
        }else{
            printf("\33[2K\rUsername o password errati!\n>");
        }
    }else if(strcmp(args[0], "sign") == 0){
        if(logged){
            printf("\33[2K\rComando non valido\n>");
            return;
        }
        if(argc < 3){
            printf("\33[2K\rsign <username> <password>\n>");
            return;
        }
        //salvo l'username
        strcpy(usr, args[1]);
        prepare_request(SIGN, args[1], args[2], buffer, usr);

        ret = send(sd, buffer, sizeof(struct Response), 0);
        if (ret < 0) {
            perror("Errore in fase di sign: ");
            exit(1);
        }
        ret = recv(sd, buffer, sizeof(struct Response), 0);
        if (ret < 0) {
            perror("Errore in fase di sign: ");
            exit(1);
        }
        convert_response(&res, buffer);
        if(res.status == SIGN_OK){
            printf("\33[2K\rSign effettuato con successo!\n>");
            logged = 1;
        }else{
            printf("\33[2K\rUsa un altro username!\n>");
        }
    }else if(!logged){ //se non sono loggato i comandi successivi non vanno considerati
        printf("\33[2K\rComando non valido\n>");
        return;
    }else if(strcmp(args[0], "list") == 0){ //richiede lista dei server
        prepare_request(LIST, "null", "null", buffer, usr);

        ret = send(sd, buffer, sizeof(struct Response), 0);
        if (ret < 0) {
            perror("Errore nella richiesta della lista: ");
            exit(1);
        }
        
        printf("\33[2K\rLista dei server disponibili:\n");
        while(1){ //richiede finche non ha interruzione
            ret = recv(sd, buffer, sizeof(struct Response), 0);
            if (ret < 0) {
                perror("Errore nelal ricezione della lista: ");
                exit(1);
            }
            convert_response(&res, buffer);
            if(res.status == LIST_END){
                break;
            }

            printf("[%d] ", atoi(res.param));
        }
        printf("\n>");
    }else if(strcmp(args[0], "connect") == 0){ //richede dati per server gioco
        if(argc < 2){
            printf("\33[2K\rconnect <id>\n>");
            return;
        }
        prepare_request(CONNECT, args[1], "null", buffer, usr);

        ret = send(sd, buffer, sizeof(struct Request), 0);
        if (ret < 0) {
            perror("Errore nella richiesta del server: ");
            exit(1);
        }

        ret = recv(sd, buffer, sizeof(struct Response), 0);
        if (ret < 0) {
            perror("Errore nella ricezione del server ");
            exit(1);
        }
        convert_response(&res, buffer);
        
        if(res.status == CON_FAILED){
            printf("\33[2K\rServer non disponibile\n>");
        }else{
            //cambio la porta
            srv_addr.sin_port = htons(atoi(res.param));
            connected = 1;
        }
        
    }

}

//funzione che converte la risposta
void convert_response(struct Response *r, char* buffer){
    sscanf(buffer, "%d %s", &r->status, r->param);
    r->status = r->status;
}

//funzione che prepara la richiesta
void prepare_request(int com, const char* param1, const char* param2, char* buffer, char* user){
    struct Request req;
    int max;
    
    if(!param1 || !param2){
        return;
    }

    req.command = com;
    //gestisco parametri e terminazioni
    strncpy(req.param1, param1, MAX_BUFFER);
    if(strlen(param1) >= MAX_BUFFER){
        max = MAX_BUFFER - 1;
    }else{
        max = strlen(param1);
    }
    req.param1[max] = '\0';
    strncpy(req.param2, param2, MAX_BUFFER);
    if(strlen(param2) >= MAX_BUFFER){
        max = MAX_BUFFER -1;
    }else{
        max = strlen(param2);
    }
    req.param2[max] = '\0';
    strcpy(req.user, user);

    sprintf(buffer, "%d %s %s %s", req.command, req.param1, req.param2, req.user);
}


