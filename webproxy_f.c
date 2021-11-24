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
int hostname_auth(struct server_conn *serv, pthread_mutex_t* lock){
    if((serv->server = gethostbyname(serv->host)) == NULL){
        if(serv->server == NULL){
            printf("NULL: %d\n", serv->servfd);
        }else{printf("BADSERVER: %s\n", serv->host);}
        return -1;
    }
    //if hostname is good, then write it to the hostname-ip cache
    char buf[300];
    struct sockaddr_in addr;
    FILE* fp;
    memcpy(&addr.sin_addr, serv->server->h_addr, serv->server->h_length);
    bzero(buf, 300);
    sprintf(buf, "%s:%s\n", serv->host, inet_ntoa(addr.sin_addr));

    pthread_mutex_lock(lock);
    fp = fopen("hostname_ip.txt", "a+");
    fprintf(fp, "%s", buf);
    fclose(fp);
    pthread_mutex_unlock(lock);
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

void compute_hash(char* url, char* hash){
    FILE* fp;
    char path[50];
    const char* a;
    char cmd[19 + strlen(url)];
    sprintf(cmd, "echo -n '%s' | md5sum", url);
    fp = popen(cmd, "r");
    while (fgets(path, 50, fp) != NULL) {
        ;
    }
    path[32] = '\0';
    strncpy(hash, path, 33);
}

void url_to_path(char* url, char* path){
    char hash[35];

    char* url_2 = malloc(strlen(url)+3);
    memcpy(url_2, url, strlen(url)+1);
    
    strcat(path, "./cache/");
    compute_hash(url_2, hash);
    strcat(path, hash);
    free(url_2);
}

void pexit(int clientfd){
    close(clientfd);
    fflush(stdout);
    pthread_exit(NULL);
}
//======================================
// Hostname - IP helpers
//======================================
/*
 * check_hostname_ip - checks the hostname-ip cache for any matching hostnames
 *          returns 1 on success
 *          returns 0 on failure
 */
int check_hostname_ip(char* host, char* ip, pthread_mutex_t* lock){
    FILE* fp;
    char buf[300];
    char* ip_2;
    char* hostname;
    int c;
    pthread_mutex_lock(lock);
    fp = fopen("hostname_ip.txt", "r");
    if(fp){
        while(fgets(buf, 300, fp)){
            hostname = strtok(buf, ":");
            if((strcmp(host, hostname)) == 0){
                ip_2 = buf + strlen(hostname)+1;
                memcpy(ip, ip_2, strlen(ip_2)-1);
                fclose(fp);
                pthread_mutex_unlock(lock);
                return 1;
            }
            bzero(buf, 300);
        }
    }else{
        perror("fopen - check_hostname_ip");
    }
    fclose(fp);
    pthread_mutex_unlock(lock);
    return 0;
}




//======================================
// Page caching
//======================================
void send_cache(int clientfd, char* url){
    char path[45];
    bzero(path, 45);
    url_to_path(url, path);

    FILE* fp;
    char buf[MAXBUF];
    int f_size, quotient, remainder;
    size_t c;
    fp = fopen(path, "rb");
    if(fp){
        fseek(fp, 0, SEEK_END);
        f_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        quotient = f_size / MAXBUF;
    	remainder = f_size % MAXBUF;
    	for(int i = 0; i < quotient; i++){
    		c = fread(buf, MAXBUF, 1, fp);
    		write(clientfd, buf, c);
            bzero(buf, MAXBUF);
    	}
    	c = fread(buf, 1, remainder, fp);
    	write(clientfd, buf, c);
    }else{
        perror("fopen - send_cache");
    }
    //printf("SENT FROM CACHE\n");
}

int cache_hit(char* url, int timeout){
    char path[45];
    bzero(path, 45);
    url_to_path(url, path);

    struct stat filestat;
    if((stat(path, &filestat)) == -1){
        perror("Stat");
    }
    time_t now = time(NULL);
    if((now - filestat.st_mtime) > timeout){
        return 0;
    }
    return 1;
    
}

/*
 * check_cache - checks page cache to see if file exists
 */
int check_cache(char* url){
    char path[45];
    bzero(path, 45);
    url_to_path(url, path);
    if(access(path, F_OK) == 0){
        return 1;
    }
    return 0; // cache miss
}

/*
 * open_file - opens cache file with the name as the md5sum 
 */
FILE* open_file(char* url){
    FILE* fp;
    char hash[35];
    char buf[1000];
    char buf2[1000];

    char* url_2 = malloc(strlen(url)+3);
    memcpy(url_2, url, strlen(url)+1);

    strcat(buf, "./cache/");
    compute_hash(url_2, hash);
    
    sprintf(buf2, "%s", hash);

    strcat(buf, buf2);
    if((fp = fopen(buf, "wb")) == NULL){
        printf("ERROR fopen: %s\n", buf);
        perror("fopen");
        //exit(-1);
        return NULL;
    }
    free(url_2);
    return fp;
}

//======================================
// Blacklist authentication
//======================================
int check_blacklist_for_ip(struct server_conn *serv, char* ip){
    FILE* fp;
    char buf[300];
    char* ip_addr;
    int c;
    if(serv->from_cache){
        ip_addr = ip;
        
    }else{
        struct sockaddr_in addr;
        memcpy(&addr.sin_addr, serv->server->h_addr, serv->server->h_length);
        ip_addr = inet_ntoa(addr.sin_addr);
    }
    ip_addr[strlen(ip_addr)] = '\0';
    fp = fopen("blacklist.txt", "r");
    if(fp){
        while(fgets(buf, 300, fp)){
            if(buf[strlen(buf)-1] == '\n'){ //cut off newlines
                buf[strlen(buf)-1] = '\0';
            }
            if((strcmp(ip_addr, buf)) == 0){
                fclose(fp);
                return -1;
            }
            bzero(buf, 300);
        }
    }else{
        perror("fopen - check_blacklist_for_ip");
    }
    fclose(fp);
    return 0;
}

int check_blacklist_for_hostname(char* hostname){
    FILE* fp;
    char buf[500];
    int c;
    fp = fopen("blacklist.txt", "r");
    if(fp){
        while(fgets(buf, 500, fp)){
            if(buf[strlen(buf)-1] == '\n'){ //cut off newlines
                buf[strlen(buf)-1] = '\0';
            }
            if((strcmp(hostname, buf)) == 0){
                fclose(fp);
                return -1;
            }
            bzero(buf, 500);
        }
    }else{
        perror("fopen - check_blacklist_for_hostname");
    }
    fclose(fp);
    return 0;
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

    if(content == NULL){
        if(strstr(buf, "chunked")){
            content = strstr(buf, "\r\n\r\n") + 4;
            content = strtok(content, "\r\n");
            return strtol(content, NULL, 16);
        }
        
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

char* read_in(char* buf, int servfd, int clientfd, int* total_len, char* serv_response){
    int d_left = MAXBUF, new = 1, n;
    char* zapp;
    int cur_len = 0;
    int content_length, header_length, total_length = 1;

    while(cur_len < total_length){
        if((n = read(servfd, buf, d_left)) < 0){
            return NULL;
        }
        if(new){
            char* buf_2 = (char* )malloc(strlen(buf)+1);
            memcpy(buf_2, buf, strlen(buf)+1);

            header_length = get_headerlength(buf_2);

            content_length = get_contentlength(buf_2);

            total_length = header_length + content_length+2;
            new = 0;
            free(buf_2);
            
            serv_response = realloc(serv_response, total_length);
            zapp = serv_response;
        }

        cur_len += n;

        write(clientfd, buf, n);

        memcpy(serv_response, buf, n);
        serv_response += n;
        bzero(buf, MAXBUF);
    }

    *total_len = total_length;
    serv_response = zapp;

    return serv_response;
}



//======================================
// Link Prefetch
//======================================
void link_prefetch(char* serv_response){
    ;
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
    }else if(type == -4){
        strcpy(buf, "HTTP/1.1 403 Forbidden\r\n\r\n");
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
int open_servfd(struct server_conn *serv, char* ip){
    int servfd;
    struct sockaddr_in serv_addr;
        
    /* Create socket descriptor */
    if((servfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Creating socket");
        return -1;
    }
    
    if(serv->from_cache){
        /*copy addr from serv struct*/
        serv_addr.sin_addr.s_addr = inet_addr(ip);
    }else{
        /*copy addr from hostent to sockaddr_in and populate sockaddr_in struct*/
        memcpy(&serv_addr.sin_addr, serv->server->h_addr, serv->server->h_length);
    }

    //printf("IP (%d): %s\n", servfd, inet_ntoa(serv_addr.sin_addr));
    serv_addr.sin_family = AF_INET;
    if((strlen(serv->port) == 0)){
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
