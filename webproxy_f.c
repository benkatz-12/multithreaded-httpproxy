#include "webproxy.h"


/*
 * edit_conn - change connection type from keep-alive to colose
 */
void edit_conn(char* body){
    char* temp;
    if((temp = strstr(body, "keep-alive")) == NULL){
        return;
    }
    memcpy(temp, "close", 5);
    memcpy(temp+5, temp+10, strlen(temp)-5);
}



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


int write_to_file(char* buf, char* url){
    FILE* fp;
    int len = strlen(buf);

    if((fp = fopen(url, "w")) == NULL){
        perror("fopen");
        return 0;
    }
    fprintf(fp, "%s", buf);

    fclose(fp);
    return 1;
}

int check_cache(struct server_conn *serv){
    if(access(serv->url, F_OK) == 0){
        //cache_hit(serv->url);
        return 1;
    }
    return 0; // cache miss
}

void pexit(int clientfd){
    close(clientfd);
    fflush(stdout);
    pthread_exit(NULL);
}






//======================================
// I/O helpers
//======================================
int get_contentlength(char* buf){
    char* temp;
    char* content = strstr(buf, "Content-Length");

    if(content == NULL){
        content = strstr(buf, "content-length");
    }

    temp = strchr(content, ':')+2;
    
    if (temp != NULL){
        return atoi(temp);
    }
    return -1;
}

int get_headerlength(char* buf_2){ //almost there
    char* temp;
    char temp2[MAXBUF];
    
    intptr_t ptr_t;
    intptr_t ptr_b;
    if((temp = strstr(buf_2, "\r\n\r\n")) == NULL){
        return -1;
    }
    ptr_t = (intptr_t)temp;
    ptr_b = (intptr_t)buf_2;
    memcpy(temp2, buf_2, ptr_t-ptr_b+2);
    temp2[ptr_t-ptr_b+2] = '\0';

    return strlen(temp2);
}

int read_in(char* buf, int servfd, int clientfd){
    int n;
    int new = 1;
    int d_left = MAXBUF;
    char* bufp = buf;
    int cur_len = 0;
    int content_length, header_length, total_length;

    while(cur_len < total_length){
        if((n = read(servfd, buf, d_left)) < 0){
            return -1;
        }
        if(new){
            char* buf_2 = (char* )malloc(strlen(buf)+1);
            memcpy(buf_2, buf, strlen(buf)+1);
            header_length = get_headerlength(buf_2);
            content_length = get_contentlength(buf_2);
            total_length = header_length + content_length+2;
            new = 0;
            free(buf_2);
        }

        cur_len += n;
        write(clientfd, buf, n);
        
        bzero(buf, MAXBUF);
    }

    return (n - d_left);
}


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
    if(type == -1){
        strcpy(buf, "HTTP/1.1 404 Not Found\r\n\r\n");
    }else if(type == -2){
        strcpy(buf, "HTTP/1.1 400 Bad Request\r\n\r\n");
    }else if(type == -3){
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