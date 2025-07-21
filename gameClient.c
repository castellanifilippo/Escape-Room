#include "gameClient.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_OBJECT 10

extern char buffer[BUFFER_SIZE];    
extern char usr[MAX_BUFFER];    //username utente
extern int sd;  //socket del server
char pending_unlock[MAX_BUFFER] = "null"; //variabile che indica l'oggeto per quale inviare risposta
char inventory[MAX_OBJECT][MAX_BUFFER]; //inventario
int items = 0;  //numero di oggetti nell'inventario
int running = 0;    //partita in corso o meno
int token = 0;  //quanti token ho
int token_tot;  //quanti token ci sono in totale

//dichiarazioni di funzioni
void game_command();
void response_handler();

//funzione principale
void gameClient(){
    fd_set master;  //descrittori da contollare
    fd_set read_fds;    //descrittori trovati dalla select
    int fdmax;  //descrittore massimo
    int ret;    //ritorno

	printf("\33[2K\rRoom disponibili:\n    Casa_Stregata\n>");
    fflush(stdout);

    //inserisco listner e standard input
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &master);
    FD_SET(sd, &master);

    fdmax = sd;
    //ciclo sulla select
    while(1){
        read_fds = master;
        ret = select(fdmax+1, &read_fds, NULL, NULL, NULL);
        if(ret<0){
            perror("ERRORE SELECT:");
            exit(1);
        }
        if(FD_ISSET(STDIN_FILENO, &read_fds)){
            //gestisco input da tastiera
            fgets(buffer, BUFFER_SIZE, stdin);
            game_command();
        }
        else if(FD_ISSET(sd, &read_fds)){
            //arrivo di una risposya
            memset(buffer, 0, BUFFER_SIZE);
            ret = recv(sd, buffer, sizeof(struct Request), 0);
            if (ret < 0) {
                close(sd);
                exit(1);
            }
            response_handler();
        }
    }
}

//comandi da standard input
void game_command(){
    char *token;    //variabile per divisione ingresso
    char *args[3];  //ingresso diviso
    int argc = 0;   //numero di ingresso
    int ret;
	int i;

    //ignoro gli invii
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
    
    //gestisco i comandi
    if(strcmp(args[0], "start") == 0){
        if(running){
            printf("\33[2K\rComando non valido\n>");
            fflush(stdout);
            return;
        }
        if(argc < 2){
            printf("\33[2K\rstart <name>\n>");
            fflush(stdout);
            return;
        }
        prepare_request(START, args[1], "null", buffer, usr);

        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di avvio: ");
            exit(1);
        }
    }else if(strcmp(args[0], "help") == 0){
        if(!running){
            printf("\33[2K\r1) start <name> --> avvia il gioco nella stanza desiderata\n>");
        }else{
            printf("\33[2K\r2) look [object|location] --> descizione della room|location|object\n");
            printf("3) take <object> --> raccogli l'oggetto \n");
            printf("4) objs --> mostra l'inventario\n");
            printf("5) use <object> [object] --> usa uno o due oggetti insieme\n");
            printf("6) end --> termina la partita\n");
            printf("7) send <word> [word] --> manda un messaggio\n>");
        }
    }else if(!running){
        printf("\33[2K\rComando non valido\n>");
        fflush(stdout);
        return;
    }else if(strcmp(args[0], "look") == 0){ //gtstisco comando llok
        if(argc < 2){
            prepare_request(LOOK, "null", "null", buffer, usr);
        }else{
            prepare_request(LOOK, args[1], "null", buffer, usr);
        }

        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di look: ");
            exit(1);
        }
    }else if(strcmp(args[0], "take") == 0){ //gestisco comando take
        if(argc < 2){
            printf("\33[2K\rtake <object>\n>");
            fflush(stdout);
            return;
        }
        strcpy(pending_unlock, args[1]);
        prepare_request(TAKE, args[1], "null", buffer, usr);

        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di take: ");
            exit(1);
        }
    }else if(strcmp(args[0], "unlock") == 0){ //gestisco risposta alla quest
        if(argc < 2){
            printf("\33[2K\runlock <answer>\n>");
            fflush(stdout);
            return;
        }
        //manda richiesta per PENDING UNLOCK
        prepare_request(UNLOCK, pending_unlock, args[1], buffer, usr);
        strcpy(pending_unlock, "");
        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di unlock: ");
            exit(1);
        }
    }else if(strcmp(args[0], "objs") == 0){ //mostro inventario
        for(i = 0; i < items; i++){
            printf("\33[2K\r%s\n", inventory[i]);
        }
        printf(">");
		fflush(stdout);
    }else if(strcmp(args[0], "use") == 0){  //gestisco la use
        if(argc < 2){
            printf("\33[2K\ruse <object> [object]\n>");
            return;
        }
        if(argc < 3){
            prepare_request(USE, args[1], "null", buffer, usr);
        }else{
            prepare_request(USE, args[1], args[2], buffer, usr);
        }
        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di use: ");
            exit(1);
        }
    }else if(strcmp(args[0], "end") == 0){ //richiedo di terminare la partita
        prepare_request(END, "null", "null", buffer, usr);
        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di end: ");
            exit(1);
        }
    }else if(strcmp(args[0], "send") == 0){ //invio messaggio ai giocatori
        if(argc < 2){
            printf("\33[2K\rsend <word> [word]\n>");
            return;
        }else if(argc <3){
             prepare_request(SEND, args[1], "null", buffer, usr);
        }else{
            prepare_request(SEND, args[1], args[2], buffer, usr);
        }
        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta di end: ");
            exit(1);
        }
    }else{
        printf("\33[2K\rComando non riconosciuto\n>");
        fflush(stdout);
    }
}

//funzione che riceve e gestisce un messaggio lungo
void getmsg(){
    int ret;
    sprintf(buffer, "%d", SEND_MSG);
    ret = send(sd, buffer, sizeof(struct Request), 0);
    if (ret < 0) {
        perror("Errore nel messaggio");
        exit(1);
    }
    //attendo il messaggio di massimo BUFFER_SIZE da stampare
    ret = recv(sd, buffer, BUFFER_SIZE, 0);
    if (ret <= 0) {
        perror("Errore nel messaggio");
        exit(1);
    }
    printf("\33[2K\r%s\n>", buffer);
    fflush(stdout);
}

//gestisco le risposte dal server
void response_handler(){
    struct Response res;
	int ret;

    convert_response(&res, buffer);
    if(res.status == START_OK){
        running = 1;
        token_tot = atoi(res.param);
        printf("\33[2K\rPartita Avviata\n>");
        fflush(stdout);
    }else if(res.status == START_FAILED){
        printf("\33[2K\rErrore nell'avvio della room\n>");
        fflush(stdout);
    }else if(res.status == NEXT_MSG){ //arriva un messaggio lungo
        getmsg();
    }else if(res.status == TIMER_UPDATE){
        printf("\33[2K\r[Tempo restante: %s min] [Token: %d/%d]\n>", res.param, token, token_tot);
        fflush(stdout);
    }else if(res.status == WIN){
        printf("\33[2K\rHai vinto!!\n>");
        fflush(stdout);
        close(sd);
        exit(0);
    }
    else if(res.status == LOSE){
        printf("\33[2K\rHai perso!!\n>");
        fflush(stdout);
        close(sd);
        exit(0);
    }else if(res.status == LOOK_FAILED){
        printf("\33[2K\rErrore nel look\n>");
        fflush(stdout);
    }
    else if(res.status == TAKE_OK){
        strcpy(inventory[items++], res.param);
        printf("\33[2K\rHai ottenuto: %s\n>", res.param);
        fflush(stdout);
    }else if(res.status == TAKE_FAILED){
        printf("\33[2K\rErrore nel take\n>");
        fflush(stdout);
    }else if(res.status == UNLOCK_OK){
        printf("\33[2K\rHai sbloccato: %s\n>", res.param);
        fflush(stdout);
    }else if(res.status == UNLOCK_FAILED){
        printf("\33[2K\rErrore nel unlock\n>");
        fflush(stdout);
    }else if(res.status == UNLOCK_ALREADY){
        printf("\33[2K\rHai già sbloccato tale oggetto\n>");
        fflush(stdout);
    }else if(res.status == USE_OK){
        printf("\33[2K\rHai ottenuto: %s\n>", res.param);
        fflush(stdout);
        strcpy(inventory[items++], res.param);
    }else if(res.status == USE_TOKEN){
        printf("\33[2K\rHai ottenuto un TOKEN!!!\n>");
        fflush(stdout);
        token++;
    }else if(res.status == USE_FAILED){
        printf("\33[2K\rErrore nella use\n>");
        fflush(stdout);
    }else if(res.status == END_GAME){
        printf("\33[2K\rPartita terminata\n>");
        fflush(stdout);
        exit(0);
    }else{
        printf("\33[2K\rRisposta non riconosciuta %d\n>", res.status);
        fflush(stdout);
    }
	//chiedo update timer
	if(res.status != TIMER_UPDATE){
		prepare_request(TIMER, "null", "null", buffer, usr);
        ret = send(sd, buffer, strlen(buffer), 0);
        if (ret < 0) {
            perror("Errore nella richiesta del timer: ");
            exit(1);
        }
	}
}

