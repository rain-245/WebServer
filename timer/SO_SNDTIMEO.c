#include<stdlib.h>
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/time.h>
#include<errno.h>

int timeout_connect(const char* addr,int port,int time){

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("socket()");
        return -1;
    }
    int ret;
    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET,addr,&serv_addr.sin_addr);

    ret = connect(sockfd,(void *)&serv_addr,sizeof(serv_addr));
    if(ret < 0){
        if(errno == EINPROGRESS){
            printf("connecttimg timeout,process timeout logic\n");
            return -1;
        }
        printf("error occuring when conneting to server");
        return -1;
    }
}

int main(int argc,char **argv){

    if(argc < 3){
        fprintf(stderr,"Usage ip port");
        exit(1);
    }
    
    int sockfd = timeout_connect(argv[1],atoi(argv[2]),10);
    if(sockfd < 0){
        return -1;
    }
    exit(0);
}