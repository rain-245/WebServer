#define _GNU_SOURCE 
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>


int main(int argc,char **argv){

    int sockfd,connfd,pipefd[2];
    struct sockaddr_in serv_addr,client_addr;
    socklen_t client_len;

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("socket()");
        exit(1);
    }

    if(argc < 3){
        fprintf(stderr,"Usage ip port\n");
        exit(1);
    }

    const char *addr = argv[1]; 
    int port = atoi(argv[2]);
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET,addr,&serv_addr.sin_addr);
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
        if(pipe(pipefd) < 0){   //创建管道
            perror("pipe()");
            close(connfd);
            exit(1);
        }
        /*将connfd上流入的客户数据定向到管道*/
        splice(connfd,NULL,pipefd[1],NULL,32678,SPLICE_F_MORE|SPLICE_F_MOVE);
        /*将管道的输出定向到connfd客户连接的文件描述符*/
        splice(pipefd[0],NULL,connfd,NULL,32678,SPLICE_F_MORE|SPLICE_F_MOVE);

        close(pipefd[0]);
        close(pipefd[1]);
        close(connfd);
    }

    close(sockfd);
    exit(0);
}