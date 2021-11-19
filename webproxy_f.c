#include "webproxy.h"





/*
 * hostname_auth - resolve hostname with gethostbyname
 *     on success populate server_conn struct
 *     on failure send back a 404 not found error
 */
int hostname_auth(struct server_conn *serv){
    //printf("Hostname")
    if((serv->server = gethostbyname(serv->host)) == NULL){
        //herror("gethostbyname");
        if(serv->server == NULL){
            printf("NULL: %d\n", serv->servfd);
        }else{printf("BADSERVER: %s\n", serv->host);}
        return -1;
    }
    return 0;
}


//======================================
// I/O helpers
//======================================
int get_totallength(char* buf){
    char* temp;
    char* content = strstr(buf, "Content-Length");
    temp = strchr(content, ':')+2;
    
    if (temp != NULL){
        return atoi(temp);
    }
    return -1;
}


int read_in(char* buf, int servfd){
    int n;
    int new = 1;
    int d_left = MAXBUF;
    char* bufp = buf;
    int content_length;
    //while contentlength > total length
    while(d_left > 0){
        if((n = read(servfd, buf, d_left)) < 0){
            return -1;
        }else if(n == 0){
            break;
        }
        d_left -= n;
        bufp += n;
        if(new){
            content_length = get_totallength(buf);
            new = 0;
        }
    }
    return (n - d_left);
}


// int write_out(char* buf, int clientfd){

// }




//======================================
// Parsing helpers
//======================================

int check_if_get(char* buf){
    char get[4];
    char cmp[] = "GET";

    strncpy(get, buf, 3);
    get[3] = '\0';

    if(strcmp(get, cmp) == 0){
        return 1;
    }
    return 0;
}



//======================================
// Error handler
//======================================


/*
 * serror - error handler routine
 */
void serror(int clientfd, int type){
    char buf[38];
    if(type == 0){
        strcpy(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
    }else if(type == 1){
        strcpy(buf, "HTTP/1.1 400 Bad Request\r\n\r\n");
    }else if(type == 2){
        strcpy(buf, "HTTP/1.1 500 Internal Proxy Error\r\n\r\n");
    }
    
    printf("client FD: %d\n",clientfd);
    write(clientfd, buf, strlen(buf));
    printf("Sent Error\n");
    close(clientfd);
    fflush(stdout);
    pthread_exit(NULL);
}


//======================================
// Opening sockets
//======================================

/*
 * open_proxyfd - create socket and initialize to accept connection on given port
 *      reutrn -1 on failure
 */
int open_proxyfd(int proxy_port){
    int proxyfd, optval=1;
    struct sockaddr_in proxyaddr;

    /* Create socket descriptor */
    if((proxyfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Creating socket");
        return -1;
    }
    
    /* Eliminates "Address already in use" error from bind. Important because we are reusing server address/port */
    if(setsockopt(proxyfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0){
        perror("Setting socket options");
        return -1;
    }
    
    /* Assign local IP address to socket */
    bzero((char *) &proxyaddr, sizeof(proxyaddr));
    proxyaddr.sin_family = AF_INET; 
    proxyaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    proxyaddr.sin_port = htons((unsigned short)proxy_port); 
    if (bind(proxyfd, (struct sockaddr*)&proxyaddr, sizeof(proxyaddr)) < 0){
        perror("Bind");
        return -1;
    }

    /* Make socket ready to accept incoming connection requests */
    if(listen(proxyfd, LISTENQ) < 0){
        perror("Listen");
        return -1;
    }
    
    return proxyfd;
}


/*
 * open_servfd - opens connection to requested http serv
 */
int open_servfd(struct server_conn *serv){
    int servfd;
    struct sockaddr_in serv_addr;
        
    /* Create socket descriptor */
    if((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Creating socket");
        return -1;
    }
    
    /*copy addr from hostent to sockaddr_in and populate sockaddr_in struct*/
    memcpy(&serv_addr.sin_addr, serv->server->h_addr, serv->server->h_length);
    //printf("IP (%d): %s\n", servfd, inet_ntoa(serv_addr.sin_addr));
    serv_addr.sin_family = AF_INET;
    if((serv->port == NULL) || (strlen(serv->port) == 0)){
        serv_addr.sin_port = htons(80);
    }else{
        serv_addr.sin_port = htons(atoi(serv->port));
    }
    
    //open TCP connection to web server
    if(connect(servfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))){
        perror("Connect");
        printf("Connect (%d)", servfd);
        return -1;
    }

    return servfd;
}