#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/fcntl.h>
#include<unistd.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/select.h>
#include<errno.h>
#include<sys/types.h>


int setnoblock(int fd){
    int oldopt;
    oldopt = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,oldopt | O_NONBLOCK);
    return oldopt;
}

int unblock_connect(const char *addr,int port,int time){

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in serv_addr;
    if(sockfd < 0){
        perror("socket()");
        return -1;
    }
    int oldopt = setnoblock(sockfd);

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET,addr,&serv_addr.sin_addr);
    int ret = connect(sockfd,(void *)&serv_addr,sizeof(serv_addr));
    
    if(ret == 0){
        printf("connect with server immediatedly!\n");
        fcntl(sockfd,F_SETFL,oldopt);
        return sockfd;
    }else if(errno != EINPROGRESS){
        /*如果连接没有立即建立，那么只有当errno为EINPROGRESS时，才表示连接还在进行，否则出错返回*/
        printf("unblock connect not support\n");
        return -1;
    }

    fd_set writefd;
    struct timeval timeout;
    FD_ZERO(&writefd);
    FD_SET(sockfd,&writefd);
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    ret = select(sockfd + 1,NULL,&writefd,NULL,&timeout);
    if(ret <= 0){
        /*select超时或者出错*/
        printf("connection time out\n");
        close(sockfd);
        return -1;
    }

    if(!FD_ISSET(sockfd,&writefd)){
        printf("no event on sockfd found\n");
        close(sockfd);
        return -1;
    }

    int error;
    socklen_t len = sizeof(error);
    /*调用getsockopt来获取并清除sockfd上的错误*/
    if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&error,&len) < 0){
        perror("getsockopt failed");
        close(sockfd);
        return -1;
    }
    /*error不为0表示出错*/
    if(error != 0){
        printf("connection failed after select with the error : %d\n",error);
        close(sockfd);
        return -1;
    }
    /*连接成功*/
    printf("connection ready after select with the socket : %d\n",sockfd);
    fcntl(sockfd,F_SETFL,oldopt);
    return sockfd;
}

int main(int argc,char **argv){
    
    if(argc < 2){
        fprintf(stderr,"Usage ip port\n");
        exit(1);
    }
    int sockfd;
    sockfd = unblock_connect(argv[1],atoi(argv[2]),10);
    if(sockfd < 0){
        exit(1);
    }
    close(sockfd);
    exit(0);
}