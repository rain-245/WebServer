#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<signal.h>
#include<errno.h>
#include<string.h>
#include<stdbool.h>

#define MAX_EVENT_NUMBER    1024

static int pipefd[2];

int setnoblock(int fd){

    int oldopt = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,oldopt|O_NONBLOCK);
    return oldopt;
}

void addfd(int epfd,int fd){

    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&event);
    setnoblock(fd);
}

void sig_handler(int sig){
    /*保留原来的errno,在函数最后恢复，保证函数的可重入性*/
    int error_save = errno;
    int msg = sig;
    send(pipefd[1],(char *)&msg,1,0);    //将信号写入管道，以通知主函数
    errno = error_save;
}

void addsig(int sig){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

int main(int argc,char **argv){

    if(argc < 3){
        fprintf(stderr,"Usage addr port");
        exit(1);
    }

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        perror("listen()");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET,argv[1],&serv_addr.sin_addr);
    if(bind(listenfd,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(listenfd,5) < 0){
        perror("listen()");
        exit(1);
    }

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epfd = epoll_create(5);
    addfd(epfd,listenfd);
    /*使用socketpair创建管道，注册pipefd[0]上的可读事件*/
    socketpair(AF_UNIX,SOCK_STREAM,0,pipefd);
    setnoblock(pipefd[1]);
    addfd(epfd,pipefd[0]);

    /*设置一些信号的处理函数*/
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;

    while(!stop_server){
        int number = epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if((number < 0) && (errno != EINTR)){
            perror("epoll_wait");
            break;
        }
        int i;
        for(i = 0;i < number;++i){
            int sockfd = events[i].data.fd;
            /*如果就绪的文件描述符是listenfd,则处理新的连接*/
            if(sockfd == listenfd){
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int connfd = accept(listenfd,(void *)&client_addr,&client_len);
            if (connfd < 0) {
                if (errno == EINTR) continue; // 被信号中断，重试
                else if (errno == EMFILE) {
                    // 处理文件描述符满的情况（比如暂时关闭一些空闲连接）
                    perror("accept: too many open files");
                    continue;
                    } else {
                        perror("accept error");
                        continue;
                    }
            }
            addfd(epfd,connfd);
            }else if(sockfd == pipefd[0] && events[i].events & EPOLLIN){
                /*如果就绪的文件描述符是pipefd[0],则处理信号*/
                int sig,ret;
                char signals[1024];
                ret = recv(sockfd,signals,1024,0);
                if(ret == -1)
                    continue;
                else if(ret == 0)
                    continue;
                else{
                    int j;
                    /*因为每个信号占一个字节，所以按字节来逐个接收信号，我们以SIGTERM为例，来说明如何安全退出主循环*/
                    for(j = 0;j < ret;++j){
                        switch(signals[j]){
                            case SIGHUP:
                            case SIGCHLD:
                                continue;
                            case SIGINT:
                            case SIGTERM:
                                stop_server = true;
                        }
                    }
                }
            }else{}
        }
    }
    printf("close fds\n");
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    exit(0);
}