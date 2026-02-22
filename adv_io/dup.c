#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>

int main(int argc,char **argv){

    int sockfd,connfd;
    struct sockaddr_in serv_addr,client_addr;
    socklen_t client_len;
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    if(argc < 2){
        perror("Usage:");
        exit(1);
    }

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("socket()");
        exit(1);
    }

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&serv_addr.sin_addr);

    if(bind(sockfd,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(sockfd,5) < 0){
        perror("listen()");
        exit(1);
    }

    client_len = sizeof(client_addr);
    connfd = accept(sockfd,(void *)&client_addr,&client_len);
    if(connfd < 0){
        perror("accept()");
        exit(1);
    }else{
        close(STDOUT_FILENO);
        dup(connfd);
        printf("abcd\n");
        close(connfd);
        sleep(2);
    }

    close(sockfd);
    exit(0);
}