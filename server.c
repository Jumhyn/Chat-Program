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

struct pthreaddata {
    int id;
};
typedef struct pthreaddata pthreaddata;

int sockfd, portno;
int clientsockfds[MAX_CONNECTIONS];
socklen_t clilens[MAX_CONNECTIONS];
struct sockaddr_in serv_addr, cli_addr[MAX_CONNECTIONS];
pthread_t threads[MAX_CONNECTIONS];
pthread_attr_t pthread_attrs[MAX_CONNECTIONS];
pthreaddata data[MAX_CONNECTIONS];
char names[MAX_CONNECTIONS][20];



void error(char *err) {
    perror(err);
    exit(0);
}

void servermsg(char *msg) {
    char *final = (char *) malloc(256);
    int n = snprintf(final, 256, "***SERVER: %s", msg);
    int i;
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        write(clientsockfds[(data[i].id)], final, strlen(final));
    }
    free(final);
}

void pushtocli(char *msg, int fromclient) {
    int i;
    char *final = (char *) malloc(256);
    int n = snprintf(final, 256, "[%s]: %s", names[fromclient], msg);
    final[n] = '\0';
    printf("Pushing %s\n", msg);
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        write(clientsockfds[(data[i].id)], final, strlen(final));
    }
    free(final);
}

void servercmd(char *msg) {
    char *cmd = malloc(10);
    char *args = malloc(246);
    sscanf(msg, "/%s %s\n", cmd, args);
    
    if (!strcmp(cmd, "quit")) {
        exit(atoi(args));
    }
}

void *opencon(void *arg) {
    pthreaddata *threaddata = (pthreaddata *)arg;
    clilens[threaddata->id] = sizeof(cli_addr[threaddata->id]);
    printf("Waiting for connection on connection %d\n", threaddata->id);
    clientsockfds[threaddata->id] = accept(sockfd, (struct sockaddr *) &cli_addr[threaddata->id], &clilens[threaddata->id]);
    printf("Connected!\n");
    write(clientsockfds[threaddata->id], "/name\n", 7);
    read(clientsockfds[threaddata->id], names[threaddata->id], 20);
    printf("Client named %s\n", names[threaddata->id]);
    if (clientsockfds[threaddata->id] < 0) 
        error("ERROR on accept");
    int n;
    char buffer[256];
    while (1) {
        bzero(buffer, 256);
        n = read(clientsockfds[threaddata->id], buffer, 255);
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

void *input(void *arg) {
    char input[256];
    while (1) {
        bzero(input, 256);
        fgets(input, 256, stdin);
        if (input[0] == '/') {
            servercmd(input);
        }
    }
}

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
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        data[i].id = i;
        pthread_attr_init(&pthread_attrs[i]);
        pthread_create(&threads[i], &pthread_attrs[i], (void *)&opencon, (void *) &data[i]);
    }
    for (i = 0; i < MAX_CONNECTIONS; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_join(inputthread, NULL);
}