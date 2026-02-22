#include<stdlib.h>
#include<stdio.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<string.h>
#include<unistd.h>

#define BUFSIZE 1024

int main(int argc,char **argv){

    int connfd,listenfd;
    fd_set read_fds,exception_fds;
    struct sockaddr_in serv_addr,client_addr;
    socklen_t client_len;
    char buf[BUFSIZE];

    if(argc < 3){
        fprintf(stderr,"Usage : ip port\n");
        exit(1);
    }
    
    int port = atoi(argv[2]);
    const char *addr = argv[1];

    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        perror("socket()");
        exit(1);
    }

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET,addr,&serv_addr.sin_addr);
    if(bind(listenfd,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(listenfd,5) < 0){
        perror("listen()");
        exit(1);
    }

    client_len = sizeof(client_addr);
    connfd = accept(listenfd,(void *)&client_addr,&client_len);
    if(connfd < 0){
        perror("accept()");
        exit(1);
    }

    int ret;
    

    while(1){
        memset(buf,'\0',BUFSIZE);
        FD_ZERO(&read_fds);
        FD_ZERO(&exception_fds);
        FD_SET(connfd,&read_fds);
        FD_SET(connfd,&exception_fds);
        ret = select(connfd + 1,&read_fds,NULL,&exception_fds,NULL);

        if(ret < 0){
            perror("select()");
            exit(1);
        }

        /*对于可读事件，采用普通的recv读取数据*/
        if(FD_ISSET(connfd,&read_fds)){
            ret = recv(connfd,buf,BUFSIZE - 1,0);
            if(ret <= 0){
                break;
            }
            printf("get %d bytes of oob data: %s",ret,buf);

        }
        /*对于异常事件，采用带有MSG_OOB标志的recv读取带外数据*/
        else if(FD_ISSET(connfd,&exception_fds)){
            ret = recv(connfd,buf,BUFSIZE - 1,MSG_OOB);
            if(ret <= 0){
                break;
            }
            printf("MSG_OOB:get %d bytes of oob data: %s",ret,buf);
        }        
    }
    close(connfd);
    close(listenfd);
    exit(0);
}