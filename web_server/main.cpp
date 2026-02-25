#include <signal.h>
#include "http_conn.h"
#include "threadpool.h"
#include "locker.h"

#define MAX_FD              65535
#define MAX_EVENT_NUMBER    10000

extern void addfd(int epollfd,int fd,bool one_shot);
extern void removefd(int epollfd,int fd);

void addsig(int sig,void (*handler)(int),bool re_start = true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    if(re_start){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);

    if(sigaction(sig,&sa,NULL) < 0){
        perror("sigaction()");
        exit(-1);
    }
}

void show_error(int fd,const char *info){
    printf("%s",info);
    send(fd,info,strlen(info),0);
    close(fd);
}

int main(int argc,char **argv){
    if(argc < 3){
        fprintf(stderr,"Usage ip port\n");
        exit(-1);
    }

    /*忽略SIGPIPE信号*/
    addsig(SIGPIPE,SIG_IGN);

    /*创建线程池*/
    thread_pool<http_conn> *pool = nullptr;
    try{
        pool = new thread_pool<http_conn>;
    }catch(...){
        return 1;
    }

    /*预先为每个可能的客户连接分配一个http_conn对象*/
    http_conn *users = new http_conn[MAX_FD];
    if(users == nullptr){
        perror("new()");
        exit(-1);
    }

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        perror("socket()");
        exit(-1);
    }
    struct linger tmp = {1,0};
    setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

    sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET,argv[1],&address.sin_addr);

    if(bind(listenfd,(struct sockaddr *)&address,sizeof(address)) < 0){
        perror("bind()");
        exit(-1);
    }

    if(listen(listenfd,5) < 0){
        perror("listen()");
        exit(-1);
    }

    int epollfd = epoll_create(5);
    if(epollfd < 0){
        perror("epoll_create()");
        exit(-1);
    }
    addfd(epollfd,listenfd,false);
    http_conn::m_epollfd = epollfd;
    epoll_event events[MAX_EVENT_NUMBER];

    while(true){
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EAGAIN){
            perror("epoll_wait()");
            break;
        }
        for(int i = 0;i < number;i++){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int connfd = accept(listenfd,(sockaddr *)&client_addr,&client_len);
                if(connfd < 0){
                    perror("accept()");
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD){
                    show_error(connfd,"Internal server busy");
                    continue;
                }
                /*初始化用户连接*/
                users[connfd].init(connfd,client_addr);
            }else if(events[i].events & (EPOLLERR | EPOLLRDHUP | EPOLLHUP)){
                /*如有异常,直接关闭客户连接*/
                users[sockfd].close_conn();
            }else if(events[i].events & EPOLLIN){
                /*根据读的结果,决定是否将任务添加到线程池,还是关闭连接*/
                if(users[sockfd].read()){
                    pool->append(users + sockfd);
                }else{
                    users[sockfd].close_conn();
                }
            }else if(events[i].events & EPOLLOUT){
                /*根据写的结果,决定是否关闭连接*/
                if(!users[sockfd].write()){
                    users[sockfd].close_conn();
                }
            }
            else{}
        }
    }
    close(listenfd);
    close(epollfd);
    delete pool;
    delete [] users;
    exit(0);
}