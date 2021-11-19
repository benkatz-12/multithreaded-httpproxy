#include "webproxy.h"

int main(int argc, char** argv){
    int proxy_port, proxyfd, *clientfdp, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid;
    

    if(argc != 2){
        fprintf(stderr, "Correct Usage: ./webproxy <port number>\n");
        exit(1);
    }

    proxy_port = atoi(argv[1]);
    
    if((proxyfd = open_proxyfd(proxy_port)) < 0){
        exit(1);
    }

    while(1){
        clientfdp = malloc(sizeof(int)); /* malloc client socket descriptor to be able to free later // clientaddr struct is not used, so overwrite is OK*/
        if((*clientfdp = accept(proxyfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0){
            perror("Accept");
            exit(1);
        }
        pthread_create(&tid, NULL, thread, clientfdp);
    }

    close(proxyfd);
}



/*
 * parse - parse HTTP request buffered at clientfd
 *       return -1 on failure
 */
int parse(int clientfd, struct server_conn *serv){
    
    int n, e;
    char buf[MAXBUF];
    const char* delim = "\r\n";
    char* request_line;
    char* method; 
    char* path;
    char* req_path;
    char* host_and_port;
    char* host;
    char* port;
    char* body;
    char* version;
    char* rl_path;
    char* cmp_method = (char*)malloc(5);
    printf("Clientfd ENTER: %d\n", clientfd);
    while((n = read(clientfd, buf, MAXBUF)) != 0){
        if(n < 0){
            perror("Read");
            return -1;
        }
        
        if(!check_if_get(buf)){
            e=-1;return e;
        }

        //printf("Server received the following request: %d bytes -  from %d : \n%s\n",n,clientfd, buf);
        //body of parsing
        
        request_line = strtok(buf, delim);
        body = buf + strlen(request_line)+2;
        method = strtok(request_line, " ");
        path = strtok(NULL, " ");
        char path_2[strlen(path)+1];
        //strcpy(path_2, path);
        memcpy(path_2, path, strlen(path)+1);
        version = strtok(NULL, " ");
        host_and_port = strtok(path, "/");
        host_and_port = strtok(NULL, "/");
        host = strtok(host_and_port, ":");
        port = strtok(NULL, ":");
        char* temp;
        //prepending '/' to any path
        req_path = strstr(path_2, host) + strlen(host);
        //printf("Req: %s  : %d\n", req_path, strlen(req_path));

        
        if(strlen(req_path) == 0){
            req_path = "/";
        }
        
        //printf("Temp: %s\n", req_path);
        

        //populate serv struct
        if(strlen(req_path) < 1000){
            strncpy(serv->path, req_path, strlen(req_path));
        }else{e=-1;return e;}
        
        if(strlen(version) <= 8){
            strncpy(serv->version, version, strlen(version));
            if(strlen(serv->version) == 9){ //hack around sometimes getting a version length of 9
                serv->version[8] = '\0';
            }
        }else{e=-1;return e;}

        if(strlen(host) < 1000){
            strncpy(serv->host, host, strlen(host));
        }else{e=-1;return e;}

        if(strlen(path) < 1000){
            strncpy(serv->path, req_path, strlen(req_path));
        }else{e=-1;return e;}

        if(port){
            if(strlen(port) < 10){
                strncpy(serv->port, port, strlen(port));
            }else{e=-1;return e;}
        }

        if(body){
            if(strlen(body) < 65535){
                strncpy(serv->body, body, strlen(body));
            }else{e=-1;return e;}
        }
        break; //FIND SOME OTHER CONDITION TO BREAK ON
    }
    printf("Clientfd EXIT: %d  -  %s\n", clientfd, serv->host);
    free(cmp_method);
    return e;
}







int get_headerlength(char* buf){  //BROKEN AS FUCK
    char* temp;
    char* content = strtok(buf, "\r\n\r\n");
    printf("header len: %d\n", sizeof(content));
    
    if (temp != NULL){
        return atoi(temp);
    }
    return -1;
}


void proxy_service(struct server_conn *serv, int clientfd){
    int n, total_len, header_len;
    int new=1;
    int cur_len = 0;
    char buf[MAXBUF];
    char request_line[11 + strlen(serv->path) + 36];
    snprintf(request_line, sizeof(request_line)+1, "GET %s %s\r\n", serv->path, serv->version);
    
    //printf("Path: %s\nPath len: %d\n", serv->path, strlen(serv->path));
    //printf("Version")
    memcpy(buf, request_line, sizeof(request_line));
    //printf("Buf: %s\n", buf);
    if(strlen(serv->body) == 0){
        //strcat(buf, "Connection: close\r\n");
        strcat(buf, "\r\n");
    }else{
        //replace Connection: keep-open -- with Connection: close //////////////////////////////////////////////
        strcat(buf, serv->body);
    }
    
    //printf("Buf: %s\n", buf);
    write(serv->servfd, buf, strlen(buf));
    bzero(buf, MAXBUF);
    // while(((n = read(serv->servfd, buf, MAXBUF)) != 0)){
    //     if(new){
    //         total_len = get_totallength(buf);
    //         //header_len = get_headerlength(buf);
    //         new = 0;
    //     }
    //     cur_len += n;
    //     printf("buflen:  %d   :   %d   :   %d/%d\n", n, clientfd, cur_len, total_len);
    //     write(clientfd, buf, strlen(buf));                                   // NEEEEED TO BREAK INTO SEPERATE PACKETS IF TOO BIG FOR BUFFER, OTHERWISE FILL BUFFER AND SEND
        
    //     bzero(buf, MAXBUF);
    //     if((cur_len > total_len)){
    //         break;
    //     }
    // }
    // printf("END    :%d\n", clientfd);
    // bzero(buf, MAXBUF);

    // total_len = MAXBUF;
    // while(cur_len < total_len){
    //     if((n = read(serv->servfd, buf, MAXBUF)) < 0){
    //         printf("Read error\n");
    //     }else if(n == 0){
    //         break;
    //     }
    //     cur_len += n;
    // }

    ///////First possible way
    //n = read_in(buf, serv->servfd);

    ///////Second possible way
    // while(1){
    //     if((n = read(serv->servfd, buf, MAXBUF)) == 0){
    //         break;
    //     }
    //     write(clientfd, buf, strlen(buf));
    // }
    

    n = read(serv->servfd, buf, MAXBUF);
    write(clientfd, buf, strlen(buf));
    printf("Server got response %d\n%s\n",n, buf);
    
    //printf("n = %d\n", n);
   
}


/* Thread routine */
void * thread(void* vargp){
    int n;
    struct server_conn serv;
    int clientfd = *((int*)vargp);
    
    pthread_detach(pthread_self()); /* Detach current thread so there is no pthread_join to wait on */
    free(vargp); /* Free clientfdp to all accepting new clients */
    
    //parse http request
    if((n=parse(clientfd, &serv)) < 0){ 
        serror(clientfd, 0);
    }

    //authenticate request
    if((n=hostname_auth(&serv)) < 0){
        serror(clientfd, 1);
    }

    //open connection to requested http server
    if((serv.servfd=open_servfd(&serv)) < 0){
        serror(clientfd, 2);
    }

    proxy_service(&serv, clientfd);

    close(clientfd);
    fflush(stdout);
    pthread_exit(NULL);
}