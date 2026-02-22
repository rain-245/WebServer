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
#include<pthread.h>

#define MAX_EVENT_NUMBER   1024
#define BUFFERSIZE         10  

struct fds{
    int fd;
    int epfd;
};

int setnoblock(int fd){
    int oldopt;
    oldopt = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,oldopt|O_NONBLOCK);
    return oldopt;
}

void addfd(int epfd,int fd,bool oneshot){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if(oneshot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&event);
    setnoblock(fd);
}

void reset_oneshot(int epfd,int fd){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&event);
}

void *work(void *argv){
    int fd = ((struct fds *)argv)->fd;
    int epfd = ((struct fds *)argv)->epfd;
    char buf[BUFFERSIZE];
    printf("start new thread to recv data on %d\n",fd);
    memset(buf,'\0',BUFFERSIZE);
    while(1){
        
        int ret = recv(fd,buf,BUFFERSIZE - 1,0);
        if(ret < 0){
            if(errno == EAGAIN){
                /*fd被设置为nonblock，因此最后一次recv调用会返回-1并设置errno*/
                reset_oneshot(epfd,fd);
                printf("read later\n");
                break;
            }
        }else if(ret == 0){
            close(fd);
            printf("forigner closed the connection\n");
            break;
        }else{
            buf[ret] = '\0';
            printf("get contents %s\n",buf);
            /*休眠5秒模拟数据处理过程*/
            sleep(5);
        }
    }
    printf("end thread recv data on %d\n",fd);
}

int main(int argc,char **argv){

    int listenfd,epfd;
    struct sockaddr_in serv_addr;
    struct epoll_event events[MAX_EVENT_NUMBER];

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
        int ret = epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if(ret < 0){
            perror("epoll_wait()");
            break;
        }

        for(int i = 0;i < ret;++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int connfd = accept(listenfd,(void *)&client_addr,&client_len);
                addfd(epfd,connfd,true);
            }else if(events[i].events & EPOLLIN){
                pthread_t thread;
                struct fds fds_for_new_worker;
                fds_for_new_worker.epfd = epfd;
                fds_for_new_worker.fd = sockfd;
                if(pthread_create(&thread,NULL,work,(void *)&fds_for_new_worker) == 0){
                    pthread_detach(thread);
                }
            }else{
                printf("something else happened\n");
            }
        }

    }

    close(listenfd);
    exit(0);

}