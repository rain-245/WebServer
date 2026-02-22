#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/types.h>
#include<errno.h>
#include<unistd.h>
#include<signal.h>
#include<assert.h>
#include "list_timer.h"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static sort_timer_lst timer_lst;
static int pipefd[2];
static int epfd;
int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addfd( int epollfd, int fd )
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
}

void sig_handler( int sig )
{
    /* 发送信号值，通过管道通知主循环 */
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

void addsig(int sig){
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

/*定时器回调函数，它删除非活动连接socket上的注册时间，并关闭之*/
void cb_func(client_data *user_data){
    epoll_ctl(epfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
}

void timer_handler()
{
    /* 处理定时事件，重新定时以不断触发SIGALRM信号 */
    timer_lst.tick();
    alarm( TIMESLOT );
}

int main(int argc,char **argv){

    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        exit( 1 );
    }

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    if( listenfd < 0 ){
        perror( "socket" );
        exit( 1 );
    }

    struct sockaddr_in serv_addr;
    memset( &serv_addr, '\0', sizeof( serv_addr ) );
    serv_addr.sin_family = AF_INET;
    inet_pton( AF_INET, argv[1], &serv_addr.sin_addr );
    serv_addr.sin_port = htons( atoi( argv[2] ) );
    if( bind( listenfd, ( struct sockaddr* )&serv_addr, sizeof( serv_addr ) ) < 0 )
    {
        perror( "bind" );
        exit( 1 );
    }

    if(listen( listenfd, 5 ) < 0)
    {
        perror( "listen" );
        exit( 1 );
    }

    epoll_event events[ MAX_EVENT_NUMBER ];
    client_data *users = new client_data[ FD_LIMIT ];
    epfd = epoll_create( 5 );
    if( epfd < 0 ){
        perror( "epoll_create" );
        exit( 1 );
    }
    addfd( epfd, listenfd );
    /* 创建管道，并将管道的读端设置为非阻塞 */
    if( socketpair( PF_UNIX, SOCK_STREAM, 0, pipefd ) < 0 ){
        perror( "socketpair" );
        exit( 1 );
    }
    setnonblocking( pipefd[1] );
    addfd( epfd, pipefd[0] );
    bool stop_server = false;
    addsig( SIGALRM );
    addsig( SIGTERM );
    alarm( TIMESLOT );
    bool timeout = false;

    while (!stop_server){
        int number = epoll_wait(epfd,events,MAX_EVENT_NUMBER,-1);
        if( ( number < 0 ) && ( errno != EINTR ) ){
            printf( "epoll failure\n" );
            break;
        }

        for(int i = 0;i < number;++i){
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                if( connfd < 0 )
                {
                    perror( "accept" );
                    continue;
                }
                addfd( epfd, connfd );
                /* 创建定时器，设置其回调函数和超时时间，并把定时器添加到链表中 */
                users[connfd].address = client_address;
                users[connfd].sockfd = connfd;
                util_timer* timer = new util_timer;
                timer->user_data = &users[connfd];
                timer->cb_func = cb_func;
                time_t cur = time( NULL );
                timer->expire = cur + 3 * TIMESLOT;
                users[connfd].timer = timer;
                timer_lst.add_timer( timer );
            }
            /*处理信号*/
            else if( ( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ) )
            {
                int sig;
                char signals[1024];
                int ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
                if( ret <= 0 )
                {
                    continue;
                }else{
                    for(int i = 0;i < ret;++i){
                        switch( signals[i] )
                        {
                            case SIGALRM:
                            {
                                /*用timeout标记有定时任务需要处理，但不立即处理定时任务，这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务*/
                                timeout = true;
                                break;
                            }
                            case SIGTERM:
                            {
                                stop_server = true;
                            }
                        }
                    }
                }
            }else if( events[i].events & EPOLLIN )
            {
                /* 这里是处理客户连接上接收到的数据的逻辑 */
                memset( users[sockfd].buf, '\0', BUFFER_SIZE );
                int ret = recv( sockfd, users[sockfd].buf, BUFFER_SIZE-1, 0 );
                printf( "get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd );
                if(ret < 0)
                {
                    /* 如果发生错误，关闭连接，并移除定时器 */
                    if( errno != EAGAIN )
                    {
                        cb_func( &users[sockfd] );
                        if( users[sockfd].timer )
                        {
                            timer_lst.del_timer( users[sockfd].timer );
                        }
                    }
                }else if( ret == 0 )
                {
                    /* 如果对方已经关闭连接，关闭连接，并移除定时器 */
                    cb_func( &users[sockfd] );
                    if( users[sockfd].timer )
                    {
                        timer_lst.del_timer( users[sockfd].timer );
                    }
                }else{
                    if(users[sockfd].timer)
                    {
                        time_t cur = time( NULL );
                        users[sockfd].timer->expire = cur + 3 * TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( users[sockfd].timer );
                    }
                }
            }
            else{
                printf( "something else happened\n" );
            }
        }
        if( timeout )
        {
            timer_handler();
            timeout = false;
        }
    }
    close( listenfd );
    close( pipefd[0] );
    close( pipefd[1] );
    delete [] users;
    exit(0);
}