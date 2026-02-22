#ifndef _PROCESSPOOL__H
#define _PROCESSPOOL__H

#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/epoll.h>
#include<signal.h>
#include<assert.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<netinet/in.h>


/*描述一个子进程类,m_pid是描述子进程的进程PID,m_pipefd是父子进程间通信的管道*/
class process{
    public:
        process():m_pid(-1){}
        pid_t m_pid;
        int m_pipefd[2];
};

/*进程池类，将它定义为模板类是为了代码复用。其模板参数是处理逻辑任务的类*/
template<typename T>
class processpool{
    public:
        static processpool<T> *create(int listenfd,int process_number = 8){
            if(!m_instance){
                m_instance = new processpool<T>(listenfd,process_number);
            }
            return m_instance;
        }

        ~processpool(){
            delete [] m_sub_process;
        }
        void run();
    
    private:
        void run_parent();
        void run_child();
        void setup_sig_pipe();
        
    private:
        processpool(int listemfd,int process_number = 8);
        /*进程池允许的最大子进程数量*/
        static const int MAX_PROCESS_NUMBER = 16;
        /*每个进程最大连接用户数目*/
        static const int MAX_USER_PER_PROCESS = 65535;
        /*epoll最多能处理的事件数目*/
        static const int MAX_EVENT_NUMBER = 10000;
        /*保存所有子进程的描述信息*/
        process *m_sub_process;
        /*进程池中进程总数*/
        int m_process_number;
        /*监听socket*/
        int m_listenfd;
        /*子进程在池中的序号,从0开始*/
        int m_idx;
        /*每个进程都有一个epoll内核事件表,m_epollfd标识*/
        int m_epollfd;
        /*子进程通过m_stop来决定是否停止允许*/
        bool m_stop;
        /*进程池静态实例*/
        static processpool<T> *m_instance;
};
template<typename T>
processpool<T> *processpool<T>::m_instance = NULL;

/*用于处理信号的管道，以实现统一事件源。后面称之为信号管道*/
static int sig_pipefd[2];

int setnonblock(int fd){
    int oldopt = fcntl(fd,F_GETFL);
    int newopt = oldopt|O_NONBLOCK;
    fcntl(fd,F_SETFL,newopt);
    return oldopt;
}

void addfd(int epollfd,int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblock(fd);
}

void sig_handler(int sig){
    /*保存errno,确保函数的可重入性*/
    int save_errno = errno;
    send(sig_pipefd[1],(char *)&sig,1,0);
    errno = save_errno;
}

void addsig(int sig,void(*handler)(int),bool restart = true){
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != -1);
}

/*从epollfd标识的epoll内核事件表中删除fd上的所有注册事件*/
static void remove_fd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

/*进程池构造函数,参数listenfd是监听socket,他必须在创建进程池之前被创建,否则子进程无法引用它。参数process_number是指定进程池中子进程的数量*/
template<typename T>
processpool<T>::processpool(int listenfd,int process_number):m_listenfd(listenfd),m_process_number(process_number),m_idx(-1),m_stop(false)
{
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));

    m_sub_process = new process[process_number];
    assert(m_sub_process);

    /*创建process_number个子进程，并建立他们和父进程之间的管道*/
    for(int i = 0;i < m_process_number;++i){
        int ret = socketpair(AF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
        if(ret < 0){
            perror("socketpair()");
            exit(-1);
        }
        
        m_sub_process[i].m_pid = fork();
        if(m_sub_process[i].m_pid < 0){
            perror("fork");
            exit(-1);
        }
        if(m_sub_process[i].m_pid > 0){
            /*父进程*/
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }else{
            /*子进程*/
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

template<typename T>
void processpool<T>::setup_sig_pipe(){
    /*创建epoll事件监听表和信号管道*/
    m_epollfd = epoll_create(5);
    if(m_epollfd < 0){
        perror("epoll_create()");
        exit(-1);
    }

    int ret = socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
    if(ret < 0){
        perror("socketpair()");
        exit(-1);
    }
    addfd(m_epollfd,sig_pipefd[0]);
    setnonblock(sig_pipefd[1]);
    /*设置信号处理函数*/
    addsig(SIGINT,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGCHLD,sig_handler);
    addsig(SIGPIPE,sig_handler);
}

/*父进程中m_idx为-1,子进程m_idx大于0,我们根据m_idx区分父子进程*/
template<typename T>
void processpool<T>::run(){
    if(m_idx == -1){
        run_parent();
        return;
    }
    run_child();
}

template<typename T>
void processpool<T>::run_child(){
    setup_sig_pipe();
    /*每个子进程都通过其在进程池中的序号值m_idx找到与父进程通信的管道*/
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    /*子进程需要监听管道文件描述符pipefd,因为父进程将通过它来通知子进程accept新连接*/
    addfd(m_epollfd,pipefd);
    T* users = new T[MAX_USER_PER_PROCESS];
    if(users == NULL){
        perror("new()");
        exit(-1);
    }
    int number = 0;
    int ret = 0;
    epoll_event events[MAX_EVENT_NUMBER];

    while(!m_stop){
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number < 0) && (errno != EINTR)){
            perror("epoll_wait");
            break;
        }
        for(int i = 0;i < number;++i){
            int sockfd = events[i].data.fd;
            /*从父子进程之间的管道读取数据,并将结果保存在变量client中。如果读取成功,则表示有新客户连接到来*/
            if((sockfd == pipefd) && (events[i].events & EPOLLIN)){
                int client = 0;
                ret = recv(pipefd,&client,sizeof(client),0);
                if((ret < 0 && errno != EAGAIN) || ret == 0){
                    continue;
                }else{
                    sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int connfd = accept(m_listenfd,(struct sockaddr *)&client_addr,&client_len);
                    if(connfd < 0){
                        perror("accept()");
                        continue;
                    }
                    addfd(m_epollfd,connfd);
                    /*模板类T必须实现init方法,以初始化一个客户连接。我们直接使用connfd来索引逻辑处理对象(T类型的对象),以提高程序效率*/
                    users[connfd].init(m_epollfd,connfd,client_addr);
                }
            }else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN){
                char signals[1024];
                ret = recv(sockfd,&signals,sizeof(signals),0);
                if(ret <= 0){
                    continue;
                }else{
                    for(int i = 0;i < ret;++i){
                        switch(signals[i]){
                            case SIGCHLD:{
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1,&stat,WNOHANG)) >0){
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                m_stop = true;
                                break;
                            }
                            default:{
                                break;
                            }
                        }
                    }
                }
            }else if(events[i].events & EPOLLIN){
                /*如果由其他数据可读,那必然是客户请求到来。调用逻辑处理对象的process方法处理之*/
                users[sockfd].process();
            }else{
                continue;
            }
        }
    }
    delete [] users;
    users = NULL;
    close(pipefd);
    //close(m_listenfd) 我们将这句话注释掉,是为了提醒读者:
    /*应该由m_listenfd的创建者来关闭这个文件描述符,即所谓的"对象"由哪个函数创建,就应该由哪个函数销毁 */
}

template<typename T>
void processpool<T>::run_parent(){
    setup_sig_pipe();
    addfd(m_epollfd,m_listenfd);
    epoll_event events[MAX_EVENT_NUMBER];
    int number,ret,connfd = 1,sub_process = 0;

    while(!m_stop){
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR){
            perror("epoll_wait()");
            break;
        }
        for(int i = 0;i < number;++i){
            int sockfd = events[i].data.fd;
            if((sockfd == m_listenfd) && (events[i].events & EPOLLIN)){
                int i = sub_process;
                do{
                    if(m_sub_process[i].m_pid != -1){
                        break;
                    }
                    i = (i + 1) % m_process_number;
                }while(i != sub_process);
                if(m_sub_process[i].m_pid == -1){
                    m_stop = true;
                    break;
                }
                sub_process = (sub_process + 1)%m_process_number;
                ret = send(m_sub_process[i].m_pipefd[0],(char *)&connfd,sizeof(connfd),0);
                // 过滤可重试错误，其他均为致命错误
                if (ret < 0 && (errno == EAGAIN || errno == EINTR)) {
                    continue; // 重试
                }
                printf("send request to child %d\n",i);
            }else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN){
                char signals[1024];
                ret = recv(connfd,signals,sizeof(signals),0);
                if(ret <= 0){
                    continue;
                }
                for(int i = ret;i < ret;++i){
                    switch(signals[i]){
                        case SIGCHLD:{
                            int stat;
                            pid_t pid;
                            while((pid = waitpid(-1,&stat,WNOHANG) ) > 0){
                                for(int i = 0;i < m_process_number;++i){
                                    if(m_sub_process[i].m_pid == pid){
                                        printf("child %d join\n",i);
                                        m_sub_process[i].m_pid = -1;
                                        close(m_sub_process[i].m_pipefd[0]);
                                    }
                                }
                            }

                            m_stop = true;
                            for(int i = 0;i < m_process_number;++i){
                                if(m_sub_process[i].m_pid != -1){
                                    m_stop = false;
                                }
                            }
                            break;
                        }
                        case SIGINT:
                        case SIGTERM:{
                            printf("kill all the child now\n");
                            for(int i = 0;i < m_process_number;++i){
                                pid_t pid = m_sub_process[i].m_pid;
                                if(pid != -1){
                                    kill(pid,SIGINT);
                                }
                            }
                            break;
                        }default:{
                            break;
                        }
                    }
                }
            }else{
                continue;
            }
        }
    }
    close(sig_pipefd[0]);
    close(m_epollfd);
}
#endif