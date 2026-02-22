#define _GNU_SOURCE 
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<poll.h>
#include<string.h>
#include<fcntl.h>

#define BUFFERSIZE 64
int main(int argc,char **argv){
    if(argc < 3){
        fprintf(stderr,"Usage : ip port\n");
        exit(1);
    }

    char buf[BUFFERSIZE];
    struct sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = atoi(argv[2]);
    inet_pton(AF_INET,argv[1],&serv_addr.sin_addr);
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("socket()");
        exit(1);
    }

    if(connect(sockfd,&serv_addr,sizeof(serv_addr)) < 0){
        perror("connect()");
        exit(1);
    }

    struct pollfd fds[2];
    int pipefd[2];
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLHUP;
    if(pipe(pipefd) < 0){
        perror("pipe");
        close(sockfd);
        exit(1);
    }

    while(1){  
        if(poll(fds,2,-1) <= 0){
            perror("poll failure");
            break;
        }

        if(fds[1].revents * POLLHUP){
            printf("server close the connection\n");
            break;
        }
        if(fds[1].revents & POLLIN){
            memset(buf,'\0',BUFFERSIZE);
            int ret = recv(sockfd,buf,BUFFERSIZE - 1,0);
            buf[ret] = '\0';
            printf("%s\n",buf);
        }
        splice(STDIN_FILENO,NULL,pipefd[1],NULL,32678,SPLICE_F_MORE|SPLICE_F_MOVE);
        splice(pipefd[0],NULL,sockfd,NULL,32678,SPLICE_F_MORE|SPLICE_F_MOVE);
    }
    close(sockfd);
    exit(0);
}