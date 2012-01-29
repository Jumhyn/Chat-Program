#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

int sockfd, portno;
struct sockaddr_in serv_addr;
struct hostent *server;
char readbuffer[256];
char writebuffer[256];
char name[20];
pthread_t writethread, readthread;
pthread_attr_t pthread_attr_read, pthread_attr_write;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void handlecmd(char *msg) {
    char *cmd = (char *) malloc(20);
    float arg;
    printf("Received command %s", msg);
    sscanf(msg, "%s/n", cmd, &arg);
    
    if (!strcmp(cmd, "/name")) {
        write(sockfd, name, 20);
    }
    
    else if (!strcmp(cmd, "pow")) {
        printf("Setting power to %.2f\n", arg);
    }
    
    free(cmd);
}

void *readt(void *arg) {
    /*continually read from stdin until 'q' is read*/
    int n;
    printf("Entering read thread...\n");
    while (1) {
        //zero out the readbuffer
        bzero(readbuffer,256);
        
        //wait for update from server
        n = read(sockfd,readbuffer,255);
        if (readbuffer[0] == 'q') {
            //quit
            break;
        }
        if (n < 0) error("ERROR reading from socket");
        if (n == 0) error("ERROR null input from server, exiting...");
        if (readbuffer[0] == '/') {
            handlecmd(readbuffer);
        }
        else {
            printf("%s",readbuffer);
        }
    }
    /*close thread*/
    printf("Read thread closing (client)\n");
    return (NULL);
}

void *writet(void *arg) {
    int n;
    printf("Entering write thread...\n");
    while (1) {
        bzero(writebuffer,256);
        //get input from stdin
        fgets(writebuffer,255,stdin);
        //push input to server
        n = write(sockfd,writebuffer,strlen(writebuffer));
        if (writebuffer[0] == 'q') {
            //quit
            break;
        }
        if (n < 0) error("ERROR writing to socket");
    }
    /*close thread*/
    printf("Write thread closing (client)\n");
    return (NULL);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    if (argc == 4)
        strcpy(name, argv[3]);
    else
        snprintf(name, 20, "client%d", rand());
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    pthread_attr_init(&pthread_attr_read);
    pthread_attr_init(&pthread_attr_write);
    
    //break off two threads, one for reading input from server, one for writing to server
    pthread_create(&writethread, &pthread_attr_write, (void *) &writet, NULL);
    pthread_create(&readthread, &pthread_attr_read, (void *) &readt, NULL);
    
    //wait for threads to complete (receive 'q' signal)
    pthread_join(readthread, NULL);
    pthread_join(writethread, NULL);
    
    printf("All threads completed, exiting...\n");

    close(sockfd);
    return 0;
}
