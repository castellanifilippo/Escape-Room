#include "gameServer.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <time.h>

#define MAX_LOCATION 5
#define MAX_OBJECT 10
#define MAX_ROOMS 10

//struttura che contiene dati sui pplayer collegati
struct Player{
    int sd; //il socket
    int connected;  //se è ancora connesso o meno
};

//descrittore di un oggetto
struct Object{
    char name[MAX_BUFFER];
    char description[BUFFER_SIZE];
    int blocked;    //se l'oggetto può essere preso o bisogna sbloccarlo
    int token;      //se rilascia un token all'utilizzo
    int can_take;   //se si può prendere
    int taken;      //se è già stato preso
    int used;       //se è già stato preso
	int usable;
    char quest[BUFFER_SIZE];    //domanda da rispondere, se presente non va sbloccato con l'uso di un altro oggetto
    char answer[MAX_BUFFER];    
    char other_obj[MAX_BUFFER]; //oggetto necessario per sbloccarlo nella use
    char prize[MAX_BUFFER];     //cosa si ottiene dopo lo sblocco
};

//descrittore della location
struct Location{
    char name[MAX_BUFFER];
    char description[BUFFER_SIZE];
};

//descrittore della stanza
struct Room{
    char name[MAX_BUFFER];
    char description[BUFFER_SIZE];
    int token;  //quanti token sono presenti
    int loc_num;    //quante locazioni
    int obj_num;    //quanti oggetti
    int timer; //tempo in minuti
    struct Location locations[MAX_LOCATION];
    struct Object objects[MAX_OBJECT];
};

//variabili globali del gioco
struct Room room;   //stanza in utlizzo
int rooms = 1;
char room_list[MAX_ROOMS][MAX_BUFFER];  //lista delle stanze (non usata, si ha una sola stanza)
time_t start_time;  //tempo di inizio della partita
int players_num = 0;    //numero di giocatori
struct Player players[MAX_PLAYERS]; //array di giocatori
fd_set master_game; //set di socket da ascolatre
int running = 0;    //variabile che indica se il gioco sta girando
int won = 0;    //varaibile di vittoria
int found_token = 0;    //numero di token trovati
struct Response res;    
int id; //id del server
struct Server *server;  //variabile server, condivisa con server principale
extern sem_t *sem;  //semaforo per accedere a server
int items = 0;  //item raccolti nell'inventario
int exclude = -1;   //socked da escludere nell'invio di messaggi


char buffer[BUFFER_SIZE];   
char inventory[MAX_OBJECT][MAX_BUFFER]; //inventario condiviso degli utenti

//funzione che crea la room di default
void defaultRoom(){
    strcpy(room.name, "Casa_Stregata");
    strcpy(room.description,
        "Ti trovi in una vecchia casa abbandonata, senti rumori tetri provenire da ogni dove.\n Vedi un enorme ++Libreria++ e un antico ++Orologio++ meccanico. Al centro della stanza si trova un tavolino con sopra un **Cofanetto** e una **Pergamena**"
    );
    room.timer = 5;
    room.token = 2;
    room.loc_num = 2;
    //location 0
    strcpy(room.locations[0].name, "Libreria");
    strcpy(room.locations[0].description,
        "La libreria non viene toccata da molto. Tra la polevere noti una **Chiave** Accanto ad un **Libro** d'orato"
    );
    room.obj_num = 5;
    //oggetto 0
    strcpy(room.objects[0].name, "Libro");
    strcpy(room.objects[0].description,
        "Un vecchio libro d'orato, il dorso del libro sembra esser stato ricucito di recente"
    );
    room.objects[0].blocked = 1;
    room.objects[0].token = 0;
    room.objects[0].can_take = 0;
    room.objects[0].taken = 0;
    room.objects[0].used = 0;
	room.objects[0].usable = 1;
    strcpy(room.objects[0].quest, "");
    strcpy(room.objects[0].other_obj, "Coltello");
    strcpy(room.objects[0].prize, "Ingranaggio");
    //oggetto 1
    strcpy(room.objects[1].name, "Chiave");
    strcpy(room.objects[1].description,
        "Una piccola chiave, con cosa va usata?"
    );
    room.objects[1].blocked = 1;
    room.objects[1].token = 0;
    room.objects[1].can_take = 1;
    room.objects[1].taken = 0;
	room.objects[1].usable = 1;
    strcpy(room.objects[1].quest, "Quante location ci sono? Rispondi con 'unlock <numero>', usa le cifre");
    strcpy(room.objects[1].answer, "2");
    strcpy(room.objects[1].prize, "Chiave");
    //location 1
    strcpy(room.locations[1].name, "Orologio");
    strcpy(room.locations[1].description,
        "Un vecchio orologio a colonna, sembra mancare un pezzo nel **Meccanismo**"
    );
    //oggetto 2
    strcpy(room.objects[2].name, "Meccanismo");
    strcpy(room.objects[2].description,
        "Un vecchio libro d'orato, il dorso del libro sembra esser stato ricucito di recente"
    );
    room.objects[2].blocked = 1;
    room.objects[2].token = 1;
    room.objects[2].can_take = 0;
    room.objects[2].taken = 0;
    room.objects[2].used = 0;
	room.objects[2].usable = 1;
    strcpy(room.objects[2].quest, "");
    strcpy(room.objects[2].other_obj, "Ingranaggio");
    strcpy(room.objects[2].prize, "");
    //oggetto 3
    strcpy(room.objects[3].name, "Cofanetto");
    strcpy(room.objects[3].description,
        "Un cofanetto placcato d'oro con una serratura, serve una chiave"
    );
    room.objects[3].blocked = 1;
    room.objects[3].can_take = 0;
    room.objects[3].token = 0;
    room.objects[3].taken = 0;
    room.objects[3].used = 0;
	room.objects[3].usable = 1;
    strcpy(room.objects[3].quest, "");
    strcpy(room.objects[3].other_obj, "Chiave");
    strcpy(room.objects[3].prize, "Coltello");
	//oggetto 4
    strcpy(room.objects[4].name, "Pergamena");
    strcpy(room.objects[4].description,
        "Una vecchia pergamena, emana una strana aura"
    );
    room.objects[4].blocked = 1;
    room.objects[4].can_take = 1;
    room.objects[4].token = 1;
    room.objects[4].taken = 0;
    room.objects[4].used = 0;
	room.objects[4].usable = 1;
    strcpy(room.objects[4].quest, "Apelle ha una palla di pelle di ... ? Rispondi con 'unlock <parola>'");
    strcpy(room.objects[4].answer, "pollo");

}


void create_list(){
	int i;
    //simula il caricamento di eventuali rooms da file
        strcpy(room_list[0], "Casa_Stregata");
    for(i = 1; i < MAX_ROOMS; i++){
        strcpy(room_list[i], "");
    }
}

//dichiarazioni di funzioni
void end_game();
int request_handler_game(int i);
void send_all(int sock, int msg);

//funzione principlae
void gameServer(struct Server *server_get, int id_get){
    int ret, listener, newfd; //funzione di ritorno, id listener e socket client
    unsigned int addrlen;   
    struct sockaddr_in my_addr, cl_addr;    //indirizzo del server e client
    time_t current_time;    //variabile usata in seguito per verificare il tempo trascorso
	int time_min;

    fd_set read_fds;    //set di descrittori da leggere
    int fdmax;  //descrittore massimo
	int i, j;

    id = id_get;
    server = server_get;

    //carico la lista delle room
    create_list();

    //creo socket e lo metto in ascolto
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    sem_wait(sem);
    my_addr.sin_port = htons(server->port);
    sem_post(sem);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(listener, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if(ret < 0){
        perror("ERRORE BIND:");
        sem_wait(sem);
        server->running = 0;
        sem_post(sem);
        exit(1);

    }

    listen(listener, 2);

    FD_SET(listener, &master_game);
    fdmax = listener;

    //ciclo sulla select
    while(1){
        read_fds = master_game;

        ret = select(fdmax+1, &read_fds, NULL, NULL, NULL);
        if(ret<0){
            perror("ERRORE SELECT:");
            exit(1);
        } 

        for(i = 0; i <= fdmax; i++){
            if(!FD_ISSET(i, &read_fds)){
                continue;
            }
            if(i == listener){
                addrlen = sizeof(cl_addr);
                // accetto la connessione e l'aggiungo al set master
                newfd = accept(listener, (struct sockaddr *)&cl_addr, &addrlen);
                if(players_num >= MAX_PLAYERS){
                    //se ci sono troppi player chiudo la connessione
                    close(newfd);
                    printf("[%d] Connessione rifiutata\n", id);
                    fflush(stdout);
                }else{
                    //altrimenti aggiungo il player ai descrittori
                    FD_SET(newfd, &master_game);
                    if(newfd > fdmax)
                        fdmax = newfd;
                    players_num++;
                    //accesso alla memoria condivisa
                    sem_wait(sem);
                    server->players = players_num;
                    sem_post(sem);
                    players[players_num - 1].sd = newfd;
                    players[players_num - 1].connected = 1;
                    printf("[%d] Nuova connessione\n", id);
                    fflush(stdout);
                }
            }else{
                // Metto la richiesta nel buffer
                memset(buffer, 0, BUFFER_SIZE);
                ret = recv(i, buffer, sizeof(struct Request), 0);
                printf("[%d] Richiesta ricevuta da %d\n", id, i);

                if (ret == 0) {
                    printf("[%d] Chiusura client rilevata\n", id);
                    fflush(stdout);
                     // il client ha chiuso il socket, quindi
                     // chiudo il socket connesso sul server
                    close(i);
                     // rimuovo il descrittore newfd da quelli da monitorare                        
                    FD_CLR(i, &master_game);
                    //lo segno disconected nell'array player
                    for(j = 0; i < players_num; j++){
                        if(players[j].sd == i){
                            players[j].connected = 0;
                            break;
                        }
                    }
                } else if (ret < 0) {
                    perror("Errore nella ricezione");
                    close(i);
                    // rimuovo il descrittore da quelli da monitorare
                    FD_CLR(i, &master_game);
                    for(j = 0; i < players_num; j++){
                        if(players[j].sd == i){
                            players[j].connected = 0;
                            break;
                        }
                    }
                } else {
                    //gestisco normalmente
                    //request_handler gestisce la richiesta e ci dice come rispondere
                    ret = request_handler_game(i);
                    if(!ret){   //non rispondo
                        continue;
                    }
					if(won){
                        res.status = WIN;
                        sprintf(buffer, "%d %s", res.status, res.param);
                        send_all(i, 1);
                        end_game();
						continue;
                    }
					if(running){
						time(&current_time);
						time_min = room.timer - ((current_time - start_time)/60);
                    	fflush(stdout);
						if(time_min <= 0){
                            //il tempo è finito
                            sprintf(buffer, "%d ", LOSE);
                            send_all(i, 1);
                            end_game();
							continue;
						}
					}
                    send_all(i, ret);
                    //verifico se la partita è stata vinta
                    if(running && res.status == END_GAME){
                        end_game();
                    }
                }
            }
        }
    }
}


void send_all(int sock, int msg){
    int ret;
    int size;
    int i = 0; 
	int j;
    int max = players_num;
    struct Response res;
    char buffertmp[sizeof(struct Response)];

    //invio a singolo utente
    if(msg == 3 || msg == 4){
        //trovo il player
        for(j = 0;j < players_num; j++){
            if(players[j].sd == sock && players[j].connected){
                i = j;
                max = j+1;
            }
        }
    }

    for(; i < max; i++){
        if(!players[i].connected || exclude == players[i].sd){
            continue;
        }
        size = sizeof(struct Response);
        //in caso di messaggio lungo avverto il client
        if(msg == 2 || msg == 4){
            res.status = NEXT_MSG;
            sprintf(buffertmp, "%d %s", res.status, res.param);
            ret = send(players[i].sd, buffertmp, size, 0);
            ret = recv(players[i].sd, buffertmp, size, 0);
            sscanf(buffertmp, "%d", &msg);
            if (ret < 0 || msg != SEND_MSG) {
                perror("Errone nell'invio! \n");
                // si è verificato un errore
                close(i);
                // rimuovo il descrittore newfd da quelli da monitorare
                FD_CLR(i, &master_game);
                players[i].connected = 0;
                continue;
            }
            size = BUFFER_SIZE;
        }
        //invio il primo/secondo messaggio
        ret = send(players[i].sd, buffer, size, 0);
        if (ret < 0) {
            perror("Errone nell'invio! \n");
            // si è verificato un errore
            close(i);
            // rimuovo il descrittore da quelli da monitorare
            FD_CLR(i, &master_game);
            players[i].connected = 0;
        }
        printf("[%d] Risposta inviata a %d %d\n", id, players[i].sd, res.status);
        fflush(stdout);
    }
    //resetto il socket da escludere
    exclude = -1;
}

//funzione che avvia la partita
int start_room(char* param){
	int i;
    //int find = 0; //varaibile usata per ricerca [qui non viene usata in quanto c'è una sola room]
    if(running){
        return 0;
    }
    //controllo se la room è disponibile
    for(i = 0; i < MAX_ROOMS; i++){
        if(strcmp(param, room_list[i]) == 0){
            //find = 1;
            break;
        }else if(strcmp("", room_list[i]) == 0){
            return 0;
        }
    }
    //carico la room default
    defaultRoom();
    time(&start_time);
    running++;
    //scrivo in meme condivisa che la partia è in corso, niente nuovi player
    sem_wait(sem);
    server->state = 1;
    sem_post(sem);
    printf("[%d] Partita avviata. Room: %s\n", id, param);
    return 1;
}

//funzione che gestisce la richiesta di look
int look(struct Request *req){
	int i;
    //se non viene specificato un oggetto/location mostro la descrizione della room
    if(strcmp(req->param1, "null") == 0){
        strcpy(buffer, room.description);
        fflush(stdout);
        return 1;
    }else{
        //cerco tra le location
        for(i = 0; i< room.loc_num; i++){
            if(strcmp(req->param1, room.locations[i].name) == 0){
                strcpy(buffer, room.locations[i].description);
                return 1;
            }
        }
        //cerco tra gli oggetti
        for(i = 0; i< room.obj_num; i++){
            if(strcmp(req->param1, room.objects[i].name) == 0){
                strcpy(buffer, room.objects[i].description);
                return 1;
            }
        }
    }
    return 0;
}

//funzione di utilità che cerca un oggetto e lo restituisce
struct Object* searchObject(char* c){
	int i;
    struct Object *o = NULL;
    for(i = 0; i < room.obj_num; i++){
        if(strcmp(room.objects[i].name, c) == 0){
            o = &room.objects[i];
            break;
        }
    }
    return o;
}

//controllo se l'oggetto è prendibile
struct Object* getObject(char* c){
    struct Object *o = searchObject(c);
    if(o){
        if(o->taken || !o->can_take){
            o = NULL;
        }
    }
    return o;
}

//funzione che gestisce il comando take
int take(struct Request *req){
    //prendo l'oggetto
    struct Object *o = getObject(req->param1);
    if(!o){
        //l'oggetto non esiste
        res.status = TAKE_FAILED;
        return 3;
    }
    if(o->blocked){
        if(strcmp(o->quest,"") == 0){
            //se non c'è una quest non si può prendere
            res.status = TAKE_FAILED;
            return 3;
        }else{
            //il player deve risolvere l'indovinello
            strcpy(buffer, o->quest);
            return 4;
        }
    }
    //se arriva qua ò'oggetto si può prendere
    strcpy(res.param, o->name);
    res.status = TAKE_OK;
    o->taken = 1;
    strcpy(inventory[items++], o->name);
    return 1;
}

//funzione  che gestisce la risposta alla domanda
void unlock(struct Request *req){
    //cerco l'oggetto
    struct Object *o = searchObject(req->param1);
    if(!o){
        //se non c'è fallisce
        res.status = UNLOCK_FAILED;
    }else if(!o->blocked){
        //già sbloccato
        res.status = UNLOCK_ALREADY;
    }else if(strcmp(o->quest, "") == 0){
        //non c'è un indovinello
        res.status = UNLOCK_FAILED;
    }else{
        //vedo se ha risolto
        if((strcmp(o->answer, req->param2) == 0)){
            res.status = UNLOCK_OK;
            strcpy(res.param, o->name);
            o->blocked = 0;
        }else{
            res.status = UNLOCK_FAILED;
        }
    }
}

//funzione che controlla se un oggetto è nell'inventario
int in_inventory(char *c){
	int i;
    for(i = 0; i < items; i++){
        if(strcmp(c, inventory[i]) == 0){
            return 1;
        }
    }
    return 0;
}

//funzione che gestisce il comando use
int use(struct Request* req){
    struct Object *o[2];    //array per i due oggetti
    int i = 0;
    char param[MAX_BUFFER]; //array di utilità
    o[0] = searchObject(req->param1);
    //vedo se c'è un secondo oggetto
    if(strcmp(req->param2,"null") != 0){
        o[1] = searchObject(req->param2);
        //controllo quale dei due deve essere usato con l'altro
        for(; i < 2 ; i++){
            if(!o[i]){
                //alcuni oggetti non hanno una struttura, esistono solo
                //come "chiave" per altri
               continue; 
            }
            if((strcmp(o[i]->quest, "")!= 0) && o[i]->blocked){
                //ha una quest
                continue;
            }
			if(!o[i]->usable){
                //non è utilizzabile
                continue;
            }
            //seleziono l'altro oggetto
            if((i+1)%2){ 
                strcpy(param, req->param2);
            }else{
                strcpy(param, req->param1);
            }
            //vedo se l'oggetto corrisponde
            if((strcmp(o[i]->other_obj, param) == 0) && in_inventory(o[i]->other_obj)){
                break;
            }
			
		
        }
        //nessun oggetto deve esseer usato con l'altro
        if(i >= 2){
            res.status = USE_FAILED;
            return 3;
        }
    }else{ //caso oggetto singolo
        if(!o[0]){
            res.status = USE_FAILED;
            return 3;
        }
        if(!o[0]->usable){
            //l'oggetto non va usato, bisogna risolvere la quest
            res.status = USE_FAILED;
            return 3;
        }
		if(!in_inventory(o[i]->name)){
			//l'oggetto non è nell'inventario'
            res.status = USE_FAILED;
            return 3;
		}
    } 
    if(o[i]->used){
        //l'oggetto utilizzabile è già stato utilizzato
        res.status = USE_FAILED;
        return 3;
    }

    //use andata a buon fine
    o[i]->used = 1;
    if(o[i]->token){ //restituisco un token
        res.status = USE_TOKEN;
        found_token ++;
        //controllo se si sono trovati tutti i token
        if(found_token >= room.token){
            won = 1;
        }
    }else{  //restituisco un oggetto
        res.status = USE_OK;
        strcpy(res.param, o[i]->prize);
        strcpy(inventory[items++], o[i]->prize);
    }
    return 1;
}

//gestisce una richiesta e prepara la risposta nel buffer
//restituisce 0 = non invia nulla | 1 = messaggio a tutti | 2 = invio di messaggio testuale a tutti |
//3 = messaggio individuale | 4 = messaggio testuale individuale
int request_handler_game(int i){
    int r;
    struct Request req;
	time_t current_time;

    convert_request(buffer, &req);
    strcpy(res.param, "");
    if(req.command == START){
        if(start_room(req.param1)){
            res.status = START_OK;
            sprintf(res.param, "%d", room.token);
        }else{
            res.status = START_FAILED;
        }
        r = 1;
    }else if(req.command == LOOK){
        if(look(&req)){
            r = 4;
        }else{
            res.status = LOOK_FAILED;
            r = 3;
        }
    }else if(req.command == TAKE){
        r = take(&req);
    }else if(req.command == UNLOCK){
        unlock(&req);
        r = 1;
    }else if(req.command == USE){
        r = use(&req);
    }else if(req.command == END){
        res.status = END_GAME;
        r = 1;
    }else if(req.command == SEND){
        //invio un messaggio ad un altro utente
        if(strcmp(req.param2, "null") == 0){
            strcpy(req.param2, "");
        }
        sprintf(buffer, "[%s] %s %s", req.user, req.param1, req.param2);
        exclude = i;
        r = 2;
    }else if(req.command == TIMER){
		//aggiorno i client sul tempo rimanente
		time(&current_time);
		r = room.timer - ((current_time - start_time)/60);	
        res.status = TIMER_UPDATE;
        sprintf(res.param, "%d", r);
        sprintf(buffer, "%d %s", res.status, res.param);
		r = 3;
	}else{
        r = 0;
    }
    //in caso di invio di response preparo il buffer
    if(r == 1 || r == 3){
        sprintf(buffer, "%d %s", res.status, res.param);
    }
    return r;
}

//funzione che termian la partita
void end_game(){
	int i;
    running = 0;
    won = 0;
    for(i = 0; i < players_num; i++){
        if(players[i].connected){
            FD_CLR(players[i].sd, &master_game);
            players[i].connected = 0;
            close(players[i].sd);
        }
    }
    players_num = 0;
    //lo segnala in mem condivisa, possibili nuove connessioni
    sem_wait(sem);
    server->state = 0;
    server->players = 0;
    sem_post(sem);
    //resetto la room
    defaultRoom();
}
