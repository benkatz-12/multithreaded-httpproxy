#include <stdio.h> /* basic IO */
#include <stdlib.h> /* exit */
#include <sys/socket.h> /* sockets */
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h> /* memset */
#include <strings.h> /* bzero, bcopy */
#include <unistd.h> /* close */
#include <pthread.h>

#define LISTENQ 10 /* Maximim number of client connections */
#define MAXBUF 65535 /* Maximum buffer size */
extern int h_errno;
struct server_conn{
    struct hostent *server;
    int servfd;
    char path[1000];
    char host[1000];
    char port[10];
    char body[65535];
    char version[9];
};

int open_proxyfd(int proxy_port); //initalize server port to accept connections
void * thread(void* vargp); //Thread routine
void serror(int clientfd, int type); //error handler 0=bad request 1=not found
int open_servfd(struct server_conn *serv);
int check_if_get(char* buf);
int hostname_auth(struct server_conn *serv);
int read_in(char* buf, int servfd);
void edit_conn(char* body);