#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

#define MAX_CONNECTIONS 20
#define MAX_MESSAGELEN 1000


//struct to hold data that is passed to each client
struct pthreaddata {
    int id;
    int connected;
};
typedef struct pthreaddata pthreaddata;

//socket and port number the server runs on
int sockfd, portno;
//client socket file descriptors
int clientsockfds[MAX_CONNECTIONS];
//client and server network stuff
socklen_t clilens[MAX_CONNECTIONS];
struct sockaddr_in serv_addr, cli_addr[MAX_CONNECTIONS];
//pthread..
pthread_t threads[MAX_CONNECTIONS];
pthread_attr_t pthread_attrs[MAX_CONNECTIONS];
pthreaddata data[MAX_CONNECTIONS];
//array of client names
char names[MAX_CONNECTIONS][20];

/*
KILL the program, while printing an arror message
*/

void error(char *err) {
    perror(err);
    exit(0);
}

/*
PUSH a message out to all connected clients, from the SERVER
*/

void servermsg(char *msg) {
    char *final = (char *) malloc(MAX_MESSAGELEN);
    int n = snprintf(final, MAX_MESSAGELEN, "***SERVER: %s", msg);
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        write(clientsockfds[(data[i].id)], final, strlen(final));
    }
    free(final);
}

/*
PUSH a message out to all connected clients, from the client with id fromclient
*/

void pushtocli(char *msg, int fromclient) {
    int i;
    char *final = (char *) malloc(MAX_MESSAGELEN);
    int n = snprintf(final, MAX_MESSAGELEN, "[%s]: %s", names[fromclient], msg);
    final[n] = '\0';
    final[n-1] = '\n';
    printf("Pushing %s", msg);
    for (i = 0; i < MAX_CONNECTIONS; i++) {
      if (data[i].connected) {
	write(clientsockfds[(data[i].id)], final, strlen(final));
      }
    }
    free(final);
}

/*
Handle a command issued to the SERVER
*/

void servercmd(char *msg) {
    char *cmd = malloc(10);
    char *args = malloc(MAX_MESSAGELEN);
    sscanf(msg, "/%s %s\n", cmd, args);
    
    if (!strcmp(cmd, "quit")) {
        exit(atoi(args));
    }
}

/*
Main CLIENT thread. Handles opening a connection and getting input from each client
*/

void *opencon(void *arg) {
    pthreaddata *threaddata = (pthreaddata *)arg;
    printf("Connected!\n");
    threaddata->connected = 1;
    write(clientsockfds[threaddata->id], "/who\n", 7);
    names[threaddata->id][read(clientsockfds[threaddata->id], names[threaddata->id], 20)] = 0;
    int i;
    for (i=0; i<MAX_CONNECTIONS; i++) {
      if (!strcmp(names[i], names[threaddata->id]) && threaddata->id != i) {
	write(clientsockfds[threaddata->id], "/newname\n", 10);
	//subtract 1 so that we overwrite the newline is overwritten
	names[threaddata->id][read(clientsockfds[threaddata->id], names[threaddata->id], 20)-1] = 0;
	i = 0;
      }
    }
    printf("Client named %s\n", names[threaddata->id]);
    if (clientsockfds[threaddata->id] < 0) 
        error("ERROR on accept");
    int n;
    char buffer[MAX_MESSAGELEN];
    while (1) {
        bzero(buffer, MAX_MESSAGELEN);
        n = read(clientsockfds[threaddata->id], buffer, MAX_MESSAGELEN-1);
        printf("Read %s", buffer);
        if (buffer[0] == '/') {
            servercmd(buffer);
        }
        pushtocli(buffer, threaddata->id);
        if (buffer[0] == '/' && buffer[1] == 'q') {
            printf("Closing %d...", clientsockfds[threaddata->id]);
            close(clientsockfds[threaddata->id]);
        }
        if (n < 0) {
            printf("ERROR reading from socket, waiting for new connection...");
            bzero(&cli_addr[threaddata->id], sizeof(cli_addr[threaddata->id]));
            clientsockfds[threaddata->id] = accept(sockfd, (struct sockaddr *) &cli_addr[threaddata->id], &clilens[threaddata->id]);
        }
        if (n == 0) {
            printf("Client %d quit, waiting for new connection...\n", clientsockfds[threaddata->id]);
            bzero(&cli_addr[threaddata->id], sizeof(cli_addr[threaddata->id]));
            clientsockfds[threaddata->id] = accept(sockfd, (struct sockaddr *) &cli_addr[threaddata->id], &clilens[threaddata->id]);
        }
    }
}

/*
Input thread for the SERVER
*/

void *input(void *arg) {
    char input[MAX_MESSAGELEN];
    while (1) {
        bzero(input, MAX_MESSAGELEN);
        fgets(input, MAX_MESSAGELEN, stdin);
        if (input[0] == '/') {
            servercmd(input);
        }
    }
}

/*
main method opens socket and starts pthreads
*/

int main(int argc, char *argv[]) {
    srand(time(NULL));
    portno = 6666;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    if (argc == 2) {
        portno = atoi(argv[1]);
    }
    printf("Opened socket\n");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }
    printf("Bound to socket\n");
    listen(sockfd, 5);
    pthread_t inputthread;
    pthread_attr_t custom_attrs;
    pthread_attr_init(&custom_attrs);
    pthread_create(&inputthread, &custom_attrs, (void *)&input, NULL);
    int i;
    while (i < MAX_CONNECTIONS) {
        clilens[i] = sizeof(cli_addr[i]);
        printf("Waiting for connection %d\n", i);
        clientsockfds[i] = accept(sockfd, (struct sockaddr *) &cli_addr[i], &clilens[i]);
        data[i].id = i;
	data[i].connected = 0;
        pthread_attr_init(&pthread_attrs[i]);
        pthread_create(&threads[i], &pthread_attrs[i], (void *)&opencon, (void *) &data[i]);
	i++;
    }
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(inputthread, NULL);
}
