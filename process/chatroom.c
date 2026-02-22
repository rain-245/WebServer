#define _GNU_SOURCE
#include<stdlib.h>
#include<stdio.h>
#include<sys/socket.h>
#include<fcntl.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<errno.h>
#include<string.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<stdbool.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<signal.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define FD_LIMIT    65535
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65535

/*处理一个客户连接必要的数据*/
struct client_data{
    struct sockaddr_in address;     /*客户端的socket地址*/
    pid_t pid;                      /*处理这个连接的子进程pid*/
    int connfd;                     /*socket文件描述符*/
    int pipefd[2];                   /*和父进程通信用的管道 子进程用pipefd[1]接收/发送数据，父进程用pipefd[0]*/
};

static const char *shm_name = "my_shm";
int sig_pipe[2];
int epfd;
int listenfd;
int shmfd;
char *share_mem = 0;

/*客户端连接数组。进程用客户连接的编号来索引这个数组，即可取得相关的客户端连接数据*/
struct client_data *users = 0;
/*子进程和客户连接的映射关系表。用进程的PID来索引这个数组，即可取得该进程所处理的客户连接的编号*/
int *sub_process = 0;
/*当前客户数量*/
int user_count = 0;
bool stop_child = false;

int setnonblock(int fd){
    int old_opt = fcntl(fd,F_GETFL);
    int new_opt = old_opt|O_NONBLOCK;
    fcntl(fd,F_SETFL,new_opt);
    return old_opt;
}

void addfd(int epfd,int fd){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN|EPOLLET;
    epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&event);
    setnonblock(fd);
}

void sig_handler(int sig){
    int errno_save = errno;
    int msg = sig;
    send(sig_pipe[1],(char *)&msg,1,0);
    errno = errno_save;
}

void addsig(int sig,void (*sig_handler)(int),bool restart){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = sig_handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    if(sigaction(sig,&sa,NULL) < 0){
        perror("sigaction()");
        exit(1);
    }
}

void del_resource(){
    close(sig_pipe[0]);
    close(sig_pipe[1]);
    close(epfd);
    close(listenfd);
    shm_unlink(shm_name);
    free(users);
    free(sub_process);
}

void children_term_handler(int sig){
    stop_child = true;
}

/*子进程运行的函数。参数inx指出该子进程处理的客户连接编号，users是保存所有客户连接数据的数组，参数share_mem指出共享内存的起始地址*/
int run_child(int idx,struct client_data *users,char *share_mem){
    struct epoll_event events[MAX_EVENT_NUMBER];
    int child_epfd = epoll_create(5);
    if(child_epfd < 0){
        perror("eopll_create()");
        exit(1);
    }
    /*子进程使用I/O复用技术来同时监听两个文件描述符：客户端socket、与父进程通信的管道文件描述符*/
    int connfd = users[idx].connfd;
    addfd(child_epfd,connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epfd,pipefd);
    int ret;
    
    addsig(SIGTERM,children_term_handler,false);
    while(!stop_child){
        int i,ret;
        int number = epoll_wait(child_epfd,events,MAX_EVENT_NUMBER,-1);
        if((number < 0) && (errno != EINTR)){
            perror("epoll_wait()");
            break;
        }
        for(i = 0;i < number;++i){
            int sockfd = events[i].data.fd;
            /*本子进程负责的客户连接有数据到达*/
            if((sockfd == connfd) && (events[i].events & EPOLLIN)){
                memset(share_mem + idx*BUFFER_SIZE,'\0',BUFFER_SIZE);
                /*将客户数据读取到对应的读缓冲中。该读缓存是共享内存的一段，
                它开始于idx*BUFFER_SIZE处，长度为BUFFER_SIZE个字节。
                因此，各个客户连接的读缓冲是共享的*/
                ret = recv(sockfd,share_mem + idx*BUFFER_SIZE,BUFFER_SIZE - 1,0);
                if(ret < 0){
                    if(errno != EAGAIN){stop_child = true;}
                }else if(ret == 0){
                    stop_child = true;
                }else{
                    /*成功读取用户数据就通知主线程(通过管道)来处理*/
                    send(pipefd,(char *)&idx,sizeof(idx),0);
                }
            }else if((sockfd == pipefd) && (events[i].events & EPOLLIN)){
                /*主进程通知本进程(通过管道)将第client个客户的数据发送到本进程负责的客户端*/
                int client = 0;
                /*接收主进程发来的数据，即有客户数据到达的连接的编号*/
                ret = recv(pipefd,(char *)&client,sizeof(client),0);
                if(ret < 0){
                    if(errno != EINTR){stop_child = true;}
                }else if(ret == 0){
                    stop_child = true;
                }else{
                    send(connfd,share_mem + client*BUFFER_SIZE,BUFFER_SIZE,0);
                }
            }else{
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epfd);
    return 0;
}
int main(int argc,char **argv){

    if(argc < 3){
        fprintf(stderr,"Usage ip port\n");
        exit(1);
    }

    int i;
    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        perror("socket()");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET,argv[1],&serv_addr.sin_addr);
    if(bind(listenfd,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(listenfd,5) < 0){
        perror("listen");
        exit(1);
    }

    user_count = 0;
    users = malloc(sizeof(struct client_data)*(USER_LIMIT + 1));
    sub_process = malloc(sizeof(int)*PROCESS_LIMIT);
    for(i = 0;i < PROCESS_LIMIT;++i){
        sub_process[i] = -1;
    }
    struct epoll_event events[MAX_EVENT_NUMBER];
    epfd = epoll_create(5);
    if(epfd < 0){
        perror("epoll_wait()");
        exit(1);
    }
    addfd(epfd,listenfd);

    if(socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipe) < 0){
        perror("sockpair()");
        exit(1);
    }
    setnonblock(sig_pipe[1]);
    addfd(epfd,sig_pipe[0]);

    addsig(SIGINT,sig_handler,true);
    addsig(SIGTERM,sig_handler,true);
    addsig(SIGPIPE,sig_handler,true);
    addsig(SIGCHLD,sig_handler,true);
    bool stop_server = false;
    bool terminate = false;
    
    /*创建共享内存，作为所有客户socket连接的读缓存*/
    shmfd = shm_open(shm_name,O_RDWR|O_CREAT,0666);
    if(shmfd < 0){
        perror("shm_open()");
        exit(1);
    }
    ftruncate(shmfd,BUFFER_SIZE*USER_LIMIT);
    share_mem = (char *)mmap(NULL,BUFFER_SIZE*USER_LIMIT,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
    if(share_mem == MAP_FAILED){
        perror("mmap()");
        close(shmfd);
        exit(1);
    }
    close(shmfd);

    while(!stop_server){
        int number = epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR){
            perror("epoll_wait()");
            break;
        }
        for(i = 0;i < number;++i){
            int sockfd = events[i].data.fd;
            if((sockfd == listenfd) && (events[i].events & EPOLLIN)){
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int connfd = accept(listenfd,(void *)&client_addr,&client_len);
                if(connfd < 0){
                    perror("perror()");
                    continue;
                }
                if(user_count >= USER_LIMIT){
                    const char *info = "too many users\n";
                    puts(info);
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }
                /*保存第user_count个客户连接的相关数据*/
                users[user_count].address = client_addr;
                users[user_count].connfd = connfd;
                /*在主进程和子进程间建立管道，以传递必要的数据*/
                if(socketpair(PF_UNIX,SOCK_STREAM,0,users[user_count].pipefd) < 0){
                    perror("socketpair()");
                    exit(1);
                }
                pid_t pid = fork();
                if(pid < 0){
                    close(connfd);
                    continue;
                }else if(pid == 0){
                    close(epfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipe[0]);
                    close(sig_pipe[1]);
                    run_child(user_count,users,share_mem);
                    munmap((void *)share_mem,USER_LIMIT*BUFFER_SIZE);
                    exit(0);
                }else{
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epfd,users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    /*记录新的客户连接在数组users中的索引值，建立进程pid和该索引值之间的映射关系*/
                    sub_process[pid] = user_count;
                    user_count++;
                }
            }else if((sockfd == sig_pipe[0] && (events[i].events & EPOLLIN))){
                /*处理信号事件*/
                int sig;
                char signals[1024];
                int ret = recv(sig_pipe[0],&signals,sizeof(signals),0);
                if(ret < 0){
                    continue;
                }else if(ret == 0){continue;}
                else{
                    for(i = 0;i < ret;++i){
                        switch(signals[i]){
                            /*子进程退出，表示有某个客户端关闭了连接*/
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1,&stat,WNOHANG) > 0)){
                                    /*用子进程的pid取得被关闭的客户连接的编号*/
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if((del_user < 0) || del_user >= USER_LIMIT){
                                        continue;
                                    }
                                    /*清除第del_user个客户连接使用的相关数据*/
                                    epoll_ctl(epfd,EPOLL_CTL_DEL,users[del_user].pipefd[0],0);
                                    close(users[del_user].pipefd[0]);
                                    /*更新users数组，将最后一个用户数据移动到当前删除的位置*/
                                    users[del_user] = users[--user_count];
                                    /*更新子进程映射表，使其指向新的用户编号*/
                                    sub_process[users[del_user].pid] = del_user;
                                }if(terminate && user_count == 0){
                                    stop_server = true;
                                    break;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                /*结束服务程序*/
                                printf("kill all the child now\n");
                                if(user_count == 0){
                                    stop_server = true;
                                    break;
                                }
                                for(i = 0;i < user_count;++i){
                                    int pid = users[i].pid;
                                    kill(pid,SIGTERM);
                                }
                                terminate = true;
                                break;
                            }
                            default:{
                                break;
                            }
                        }
                    }
                    
                }
            }else if(events[i].events & EPOLLIN){
                /*某个子进程向父进程写入了数据*/
                int child = 0;
                /*读取管道数据,child变量记录了是哪个客户连接有数据到达*/
                int ret = recv(sockfd,(char *)&child,sizeof(child),0);
                printf("read data from child across pipe\n");
                if(ret == -1){
                    continue;
                }else if(ret == 0){
                    continue;
                }else{
                    /*向除负责处理第child个客户连接的子进程之外的其他子进程发送消息，通知他们有客户数据要写*/
                    for(int j = 0;j < user_count;++j){
                        if(users[j].pipefd[0] != sockfd){
                            printf("send data to child across pipe\n");
                            send(users[j].pipefd[0],(char *)&child,sizeof(child),0);
                        }
                    }
                }
            }
        }
    }
    del_resource();
    exit(0);
}