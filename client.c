#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <ncurses.h>

#define MAX_MESSAGELEN 2000

int sockfd, portno;
struct sockaddr_in serv_addr;
struct hostent *server;
char readbuffer[MAX_MESSAGELEN];
char writebuffer[MAX_MESSAGELEN];
char name[20];
pthread_t writethread, readthread;
pthread_attr_t pthread_attr_read, pthread_attr_write;

int cur_x, cur_y;
int max_x, max_y;
int lines = 0;


void error(const char *msg)
{
    perror(msg);
    endwin();
    exit(0);
}

void handlecmd(char *msg) {
    char *cmd = (char *) malloc(20);
    float arg;
    printw("Received command %s", msg);
    sscanf(msg, "%s/n", cmd, &arg);
    
    if (!strcmp(cmd, "/who")) {
        printw("Identifying as %s\n", name);
	refresh();
        write(sockfd, name, 20);
    }
    else if (!strcmp(cmd, "/newname")) {
      printw("That name is already in use! Please enter a different name.\n");
      refresh();
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
      bzero(readbuffer, MAX_MESSAGELEN);
        
        //wait for update from server
        n = read(sockfd,readbuffer,MAX_MESSAGELEN-1);
        if (!strcmp(readbuffer, "/q\n")) {
            //quit
            break;
        }
	getyx(stdscr, cur_y, cur_x);
        if (n < 0) error("ERROR reading from socket");
        if (n == 0) error("ERROR null input from server, exiting...");
        if (readbuffer[0] == '/') {
            handlecmd(readbuffer);
        }
        else {
	    getmaxyx(stdscr, max_y, max_x);
	    if (lines == max_y-1) {
		scroll(stdscr);
	    }
	    else {
		lines++;
	    }
	    mvprintw(lines-1, 0, "%s",readbuffer);
      	    move(cur_y, cur_x);
	    printw("%s", writebuffer);
	    refresh();
        }
    }
    /*close thread*/
    printf("Read thread closing (client)\n");
    return (NULL);
}

void *writet(void *arg) {
    int n;
    printf("Entering write thread...\n");
    scrollok(stdscr, true);
    while (1) {
        bzero(writebuffer,256);
        //get input from stdin
	int c, i=0;
	while((writebuffer[i] = c = getch()) != EOF && c != '\n') {
	  int w, h;
	  getmaxyx(stdscr, h, w);
	  getyx(stdscr, cur_y, cur_x);
	  move(h-1, cur_x);
	  if (c == KEY_BACKSPACE || c == KEY_DC || c == 127) {
	    i--;	   
	    delch();
	    getyx(stdscr, cur_y, cur_x);
	    move(cur_y, cur_x-1);
	    writebuffer[i] = 0;
	  }
	  else {
	    i++;
	    addch(c);
	  }
	  refresh();
	}
	writebuffer[i++] = '\n';
	writebuffer[i] = '\0';
	move(cur_y, 0);
	clrtoeol();
	move(cur_y, 0);
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
    initscr();
    cbreak();
    noecho();
    getmaxyx(stdscr, max_y, max_x);
    srand(time(NULL));
    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port username\n", argv[0]);
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
    endwin();
    return 0;
}
