#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>    
#include <fcntl.h>        
#include <semaphore.h>
#include <signal.h>
#include <sys/wait.h>
#include "gameServer.h"

struct User{
    char username[MAX_BUFFER];
    char password[MAX_BUFFER];
};

int running_servers = 0; //numero di partite in corso
int listener, newfd;    //descrittori di socket

struct Server *servers; //array server in mem condivisa
sem_t *sem; //semaforo per mem condivisa
fd_set master;  //set descrittori da controllare
int fdmax;  //massimo dei desc
pid_t child_pids[MAX_SERVERS];  //pid dei processi figli

//dichiarazioni di funzioni
void handle_signal(int signal);
void comandi(char* buffer);
void help();
int request_handler(char* buffer, int i);
void start_server(int id, int port);
void restart_server(int i, int port);


int main(int argc, char **argv){

    int port, ret; //variabile della porta e di ritorno
    unsigned int addrlen;
    int shm_fd; //variabile mem condivisa
    struct sockaddr_in my_addr, cl_addr; //indirizzo server e client

    fd_set read_fds;    //descrittori da controllare nel ciclo

    char buffer[BUFFER_SIZE];
    char sem_name[256]; //nome del semaforo

	int i;

    //segnale che termina i processi figli
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);

    //prendo la porta dagli argomenti se presente
    if(argc < 2){
        printf("Inserisci una porta\n");
        exit(1);
    }else{
        if (sscanf(argv[1], "%d", &port) != 1){
            printf("Parametro non valido");
            exit(1);
        }
    }
    //inizializzo lista server di gioco
    // creo/apro la memoria condivisa
    shm_fd = shm_open("server", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // configuro la dimensione della memoria condivisa
    if (ftruncate(shm_fd, (MAX_SERVERS) * sizeof(struct Server)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    // mappo la memoria condivisa
    servers = (struct Server*)mmap(0, MAX_SERVERS * sizeof(struct Server), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (servers == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // creo/apro il semaforo con nome unico
    snprintf(sem_name, sizeof(sem_name), "%s_%d", "retiInfSem", getpid());
    sem = sem_open(sem_name, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }

    //inizializzo i server
    for(i = 0; i < MAX_SERVERS ; i++){
        servers[i].players = 0;
        servers[i].state = 0;
        servers[i].running = 0;
    }

    //inizializzo il socket del server principale
    listener = socket(AF_INET, SOCK_STREAM, 0);

    //instanzio sock_addr_in
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    //eseguo il binding e gestisco eventuale errore
    ret = bind(listener, (struct sockaddr*)&my_addr, sizeof(my_addr));

    if(ret < 0){
        perror("ERRORE BIND:");
        exit(1);
    }

    //metto in ascolto con una coda massima di 10
    listen(listener, 10);

    //inserisco listner e standard input
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &master);
    FD_SET(listener, &master);

    fdmax = listener;

    printf("********************** SERVER STARTED **********************\n");
    printf("Digita un comando:\n\n");
    help();
    printf("************************************************************\n");
    
    //ciclo della select
    while(1){
        read_fds = master;

        ret = select(fdmax+1, &read_fds, NULL, NULL, NULL);
        if(ret<0){
            perror("ERRORE SELECT:");
            exit(1);
        }
        //scorro i vari descrittori
        for(i = 0; i <= fdmax; i++){
            if(FD_ISSET(i, &read_fds)){
                //caso del listner
                if(i == listener){
                    printf("Nuova connessione!\n");
                    fflush(stdout);
                    addrlen = sizeof(cl_addr);

                    // accetto la connessione e l'aggiungo al set master
                    newfd = accept(listener, (struct sockaddr *)&cl_addr, &addrlen);
                    FD_SET(newfd, &master);
                    if(newfd > fdmax)
                        fdmax = newfd;
                }else if(i == STDIN_FILENO){
                    //nuovo input da tastiera
                    fgets(buffer, BUFFER_SIZE, stdin);
                    comandi(buffer);

                }else{
                    // Metto la richiesta nel buffer
                    memset(buffer, 0, BUFFER_SIZE);
                    ret = recv(i, buffer, sizeof(struct Request), 0);

                    if (ret == 0) {
                        printf("CHIUSURA client rilevata!\n");
                        fflush(stdout);
                         // il client ha chiuso il socket
                         // chiudo il socket connesso sul server
                        close(i);
                         // rimuovo il descrittore da quelli da monitorare
                        FD_CLR(i, &master);
                    } else if (ret < 0) {
                        perror("Errore nella ricezione! \n");
                         // si è verificato un errore
                        close(i);
                         // rimuovo il descrittore da quelli da monitorare
                        FD_CLR(i, &master);
                    } else {
                        printf("Richiesta del client rilevata\n");
                        fflush(stdout);
                        //eseguo handler e vedo se devo inviare qualcosa
                        if(!request_handler(buffer, i)){
                            continue;
                        }
                        ret = send(i, buffer, sizeof(struct Response), 0);
                        if (ret < 0) {
                            perror("Errone nell'invio! \n");
                            // si è verificato un errore
                            close(i);
                            // rimuovo il descrittore da quelli da monitorare
                            FD_CLR(i, &master);
                        }
                    }

                }

            }
        }

    }

}

//funzione che gestisce i comandi da tastiera
void comandi(char* buffer){
    char *token;    //variabile per divisione ingresso
    char *args[3];  //ingresso suddiviso
    int argc = 0;   //numero di ingressi
    int port;       //porta server
    int ret;   
	int i;     

    //ignoro invio a capo
    if(strcmp(buffer,"\n") == 0){
        return;
    }

    //tolgo \n dall'ingresso
    buffer[strlen(buffer) -1] = '\0';

    // divido la stringa
    token = strtok(buffer, " ");
    while (token != NULL && argc < 3) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }

    //gestisco il comando
    if(strcmp(args[0],"start") == 0){
        if(running_servers >= 10){
            printf("Limite di server attivi raggiunto\n");
        }
        else if(argc < 2){
            printf("Inserisci una porta\n");
            printf(" start <port>\n");
        }else{
            if (sscanf(args[1], "%d", &port) != 1){
                printf("Parametro non valido\n");
            }else{
                start_server(running_servers++, port);
            }
        }
    }else if(strcmp(args[0],"stop") == 0){
        //verifico se ci sono partite in corso
        ret = 0;
        sem_wait(sem);
        for(i = 0; i < running_servers; i++){
            if(servers[i].state){
                ret = 1;
                break;
            }
        }
        sem_post(sem);
        if(ret){
            printf("Impossibile arrestare il server: ci sono partite in corso\n");
			fflush(stdout);
        }else{
            for(i = 0; i < fdmax; i++){
                if(FD_ISSET(i, &master)){
                    close(i);
                }
            }
            sem_close(sem);
            //termino i processi figli
            handle_signal(SIGTERM);
            exit(0);
        }
    }else if(strcmp(args[0],"help") == 0){
        help();
    }else if(strcmp(args[0],"list") == 0){ //stampo lista server attivi
        sem_wait(sem);
        for(i = 0; i < running_servers; i++){
            printf("Server [%d]:\n", i);
            printf("    Porta: %d\n", servers[i].port);
            printf("    Partita: ");
            if(servers[i].state){
                printf("avviata\n");
            }else{
                printf("in attesa\n");
            }
            printf("    Giocatori: %d/%d\n", servers[i].players, MAX_PLAYERS);
            printf("    Stato: ");
            if(servers[i].running){
                printf("attivo\n");
            }else{
                printf("SPENTO!!!\n");
            }
        }
        sem_post(sem);
        fflush(stdout);
    }else if(strcmp(args[0],"restart") == 0){ //riavvio un server di gioco cambiandogli la porta
        if(argc < 3){
            printf("restart <id> <port>\n");
        }else{
            restart_server(atoi(args[1]), atoi(args[2]));
        }
    }else{
        printf("Comando non riconosiuto\n");
        help();
    }
}

//funzione che stampa i possibili comandi
void help(){    
    printf("1) start <port> --> avvia il server di gioco\n");
    printf("2) restart <id> <port> --> riavvia un server su una nuova porta\n");
    printf("3) list --> lista dei server avviati\n");
    printf("4) stop --> termina il server\n");
}

//funzione che gestisce il login
int login(char* username, char* password){
    FILE *file = fopen("files/users.txt", "r");
    struct User user;

    //verifico apertura file
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        return 0;
    }

    //creco nome e password
    while (fscanf(file, "%s %s", user.username, user.password) != EOF) {
        if ((strcmp(user.username, username) == 0) && (strcmp(user.password, password) == 0)) {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

//funzione che hestisce login
int sign(char* username, char* password) {
    FILE *file = fopen("files/users.txt", "a+");
    struct User user;

    //verifco aperura file
    if (file == NULL) {
        printf("Errore nell'apertura del file!\n");
        return 0;
    }

    //controllo se l'user c'è già
    while (fscanf(file, "%s %s", user.username, user.password) != EOF) {
        if ((strcmp(user.username, username) == 0)) {
            fclose(file);
            return 0;
        }
    }

    //salvo le credenziali
    fprintf(file, "%s %s\n", username, password);
    fflush(stdout);
    fclose(file);
    return 1;
}

//invia la lista dei server disponibili
int send_list(char *buffer, int sd){
    int ret;
	int i;
    sem_wait(sem);
    for(i = 0; i < running_servers; i++){
        if((servers[i].running == 1) && (servers[i].state == 0)){
            sprintf(buffer, "%d %d", LIST_PARAM, i);
            //invio il server
            ret = send(sd, buffer, sizeof(struct Response), 0);
            if (ret < 0) {
                perror("Errone nell'invio! \n");
                // si è verificato un errore
                close(i);
                // rimuovo il descrittore da quelli da monitorare
                FD_CLR(i, &master);
                sem_post(sem);
                return 0;
            }
        }
    }
    sem_post(sem);
    return 1;
}

//funzione che connette il player ad un server (gli invia la nuova porta)
int connect_game(int ser){
    int port;
    //verifico se presente
    if(ser >= running_servers){
        return 0;
    }
    sem_wait(sem);
    //verifico se la partita è già iniziata e se ci sono posti
    if(!servers[ser].running || servers[ser].state ||(servers[ser].players >= MAX_BUFFER)){
        return 0;
    }
    port = servers[ser].port;
    sem_post(sem);
    return port;
}

//gestisce una richiesta e prepara la risposta nel buffer
int request_handler(char *buffer, int i){
    struct Response res;
    struct Request req;
    int ret;
    convert_request(buffer, &req);
    strcpy(res.param, "");
    if(req.command == LOGIN){
        if(login(req.param1, req.param2)){
            res.status = LOG_OK;
        }else{
            res.status = LOG_FAILED;
        }
    }else if(req.command == SIGN){
        if(sign(req.param1, req.param2)){
            res.status = SIGN_OK;
        }else{
            res.status = SIGN_FAILED;
        }
    }else if(req.command == LIST){
        if(!send_list(buffer, i)){
            return 0;
        }
        res.status = LIST_END;
    }else if(req.command == CONNECT){
        ret = connect_game(atoi(req.param1));
        if(ret){
            res.status = CON_OK;
            sprintf(res.param, "%d", ret);
        }else{
            res.status = CON_FAILED;
        }
    }else{
        return 0;
    }
    sprintf(buffer, "%d %s", res.status, res.param);
    return 1;
}

//funzione che converte una request in text
void convert_request(char* buffer, struct Request *r){
    sscanf(buffer, "%d %s %s %s", &r->command, r->param1, r->param2, r->user);
    fflush(stdout);
    r->command = r->command;
    r->param1[MAX_BUFFER - 1] = '\0';
    r->param2[MAX_BUFFER - 1] = '\0';
    r->user[MAX_BUFFER - 1] = '\0';
}

void handle_signal(int signal) {
    // Termina tutti i processi figli
	int i;
    for (i = 0; i < running_servers; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
            waitpid(child_pids[i], NULL, 0);
        }
    }
    exit(0);
}

//funzione che avvia un server
void start_server(int id, int port){
    int p;
    p = fork();
    if (p == 0) {
        // Creo un processo per il nuovo server di gioco
        // Prima chiudo i socket che non ci serviranno
        close(listener);
        close(newfd);
        sem_wait(sem);
        servers[id].port = port;
        servers[id].running = 1;
        servers[id].state = 0;
        servers[id].players = 0;
        sem_post(sem);
        gameServer(&servers[id], id);
        exit(0);
    }
    child_pids[id] = p;
    printf("Avvio del server\n");
    fflush(stdout);

}

//funzione che riavvia il server di gioco con nuova porta
void restart_server(int i, int port){
    if(i > running_servers){
        printf("Server inesistente\n");
    }
    kill(child_pids[i], SIGTERM);
    waitpid(child_pids[i], NULL, 0);
    start_server(i, port);
}
