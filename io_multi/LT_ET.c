#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<string.h>
#include<stdbool.h>
#include<fcntl.h>
#include<errno.h>

#define MAX_EVENT_NUMBER   1024
#define BUFFERSIZE         10   

int setnoblock(int fd){
    int oldopt;
    oldopt = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,oldopt|O_NONBLOCK);
    return oldopt;
}

void addfd(int epfd,int fd,bool is_ET){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(is_ET){
        event.events |= EPOLLET;
    }
    epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&event);
    setnoblock(fd);
}

void LT(int epfd,int listenfd,int number,struct epoll_event *events){
    char buf[BUFFERSIZE];
    int i,sockfd,connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    for(i = 0;i < number;++i){
        sockfd = events[i].data.fd;
        /*接收连接请求*/
        if(sockfd == listenfd){
            connfd = accept(listenfd,(void *)&client_addr,&client_len);
            addfd(epfd,connfd,false);
        }else if(events[i].events & EPOLLIN){
            int ret;
            memset(buf,'\0',BUFFERSIZE);
            ret = recv(sockfd,buf,BUFFERSIZE - 1,0);
            if(ret <=0){
                close(sockfd);
                continue;
            }
            printf("recv %d bytes of contents: %s\n",ret,buf);

        }else{
            printf("something else happened\n");
        }
    }
}

void ET(int epfd,int listenfd,int number,struct epoll_event *events){
    char buf[BUFFERSIZE];
    int i,sockfd,connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    for(i = 0;i < number;++i){
        sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            connfd = accept(listenfd,(void *)&client_addr,&client_len);
            addfd(epfd,connfd,true);
        }else if(events[i].events & EPOLLIN){
            printf("event trigger once\n");
            while(1){
                int ret;
                memset(buf,'\0',BUFFERSIZE);
                ret = recv(sockfd,buf,BUFFERSIZE - 1,0);
                if(ret < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK){
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }else if(ret == 0){
                    close(sockfd);
                    break;
                }else{
                    printf("get %d bytes of contents : %s\n",ret,buf);
                }
            }
            
        }else{
            printf("something else happened\n");
        }
    }
}

int main(int argc,char **argv){

    int listenfd,epfd;
    struct sockaddr_in serv_addr;
    struct epoll_event events[MAX_EVENT_NUMBER];
    int ret;
    if(argc < 3){
        fprintf(stderr,"Usage : ip port\n");
        exit(1);
    }
    const char *ip = argv[1];
    int port = htons(atoi(argv[2]));

    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        perror("socket()");
        exit(1);
    }

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = port;
    inet_pton(AF_INET,ip,&serv_addr.sin_addr);

    if(bind(listenfd,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(listenfd,5) < 0){
        perror("listen()");
        exit(1);
    }

    epfd = epoll_create(5);

    addfd(epfd,listenfd,false);

    while(1){
        ret = epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if(ret <= 0){
            perror("epoll failed");
            break;
        }
        //LT(epfd,listenfd,ret,events);       
        ET(epfd,listenfd,ret,events);
    }

    close(listenfd);
    exit(0);

}