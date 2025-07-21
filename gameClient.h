#define BUFFER_SIZE 1024
#define MAX_BUFFER 20

//risposte dal server
#define LOG_OK 1
#define LOG_FAILED 2
#define SIGN_OK 3
#define SIGN_FAILED 4
#define LIST_PARAM 5
#define LIST_END 6
#define CON_OK 7
#define CON_FAILED 8
#define START_OK 9
#define START_FAILED 10
#define NEXT_MSG 11
#define TIMER_UPDATE 12
#define WIN 13
#define LOSE 14
#define LOOK_FAILED 15
#define TAKE_OK 16
#define TAKE_FAILED 17
#define UNLOCK_OK 18
#define UNLOCK_FAILED 19
#define UNLOCK_ALREADY 20
#define USE_OK 21
#define USE_TOKEN 22
#define USE_FAILED 23
#define END_GAME 24

//richieste
#define LOGIN 1
#define SIGN 2
#define LIST 3
#define CONNECT 4
#define START 5
#define LOOK 6
#define TAKE 7
#define UNLOCK 8
#define USE 9
#define END 10
#define SEND 11
#define TIMER 12
#define SEND_MSG 99

//struttura richiesta
struct Request{
    int command;
    char param1[MAX_BUFFER];
    char param2[MAX_BUFFER];
    char user[MAX_BUFFER];
};

//struttura risposta
struct Response{
    int status;
    char param[MAX_BUFFER];
};

//dichiarazioni di funzioni
void gameClient();
void convert_response(struct Response *r, char* buffer);
void prepare_request(int com, const char* param1, const char* param2, char* buffer, char* user);
