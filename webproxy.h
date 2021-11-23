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
#include <sys/stat.h>
#include <time.h>

#define LISTENQ 10 /* Maximim number of client connections */
#define MAXBUF 65535 /* Maximum buffer size */
extern int h_errno;
struct args{
    int* clientfdp;
    int timeout;
    pthread_mutex_t* lock;
};

struct server_conn{
    struct hostent *server;
    int from_cache;
    int servfd;
    char url[1000];
    char path[1000];
    char host[1000];
    char port[10];
    char body[65535];
    char version[9];
};

int open_proxyfd(int proxy_port); //initalize server port to accept connections
void * thread(void* vargp); //Thread routine
void serror(int clientfd, int type); //error handler 0=bad request 1=not found
int open_servfd(struct server_conn *serv, char* ip);
int check_if_get(char* buf);
int hostname_auth(struct server_conn *serv, pthread_mutex_t* lock);
char* read_in(char* buf, int servfd, int clientfd, int* total_len, char* serv_response);
void edit_conn(char* body);
int check_cache(char* url);
void pexit(int clientfd);
FILE* open_file(char* url);
int cache_hit(char* url, int timeout);
void send_cache(int clientfd, char* url);
int check_blacklist_for_hostname(char* hostname);
int check_hostname_ip(char* host, char* ip, pthread_mutex_t* lock);
//int check_blacklist_for_ip(struct server_conn *serv, char* ip);