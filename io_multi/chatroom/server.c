#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<fcntl.h>
#include<poll.h>
#include<sys/types.h>
#include<errno.h>

#define USER_LIMT        5
#define FD_LIMIT        65535
#define BUFFER_SIZE     64

struct client_data
{
    struct sockaddr_in address;
    char *write_buf;
    char buf[BUFFER_SIZE];
};


int setnoblock(int fd){
    int oldopt;
    oldopt = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,oldopt | O_NONBLOCK);
    return oldopt;
}

int main(int argc,char **argv){

    if(argc < 3){
        fprintf(stderr,"Usage addr port\n");
        exit(1);
    }

    int listenfd;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
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
        perror("listen()");
        exit(1);
    }

    /*创建Users数组，分配FD_LIMIT个client_data对象。
    每个socket连接都可以获得一个这样的对象，并用socket的值来作为下标索引对应的client_data对象
    这是将socket和客户端关联的简单而高效的方式*/
    struct client_data *Users = (struct client_data *)malloc(sizeof(struct client_data)*FD_LIMIT);
    /*监听事件数组，fds[0]为listenfd*/
    struct pollfd fds[USER_LIMT + 1];
    int i;
    fds[0].fd = listenfd;
    fds[0].events = POLLIN|POLLERR;
    for(i = 1;i <= USER_LIMT;++i){
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    int user_counter = 0;

    int ret;
    while (1)
    {   
        int i,j;
        /*user_counter为当前已建立连接的用户数，+1是监视listenfd来查看新连接*/
        ret = poll(fds,user_counter + 1,-1);
        if(ret < 0){
            perror("poll");
            break;
        }
        for(i = 0;i < user_counter + 1;++i){
            /*处理新连接*/
            if((fds[i].fd == listenfd) && (fds[i].revents & POLLIN)){
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int connfd = accept(listenfd,(void *)&client_addr,&client_len);
                if(connfd < 0){
                    perror("accept()");
                    continue;
                }
                /*如果请求太多，则关闭新的连接*/
                if(user_counter >= USER_LIMT){
                    const char *info = "too many users\n";
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }
                user_counter++;
                Users[connfd].address = client_addr;
                setnoblock(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN|POLLERR|POLLRDHUP;
            }else if(fds[i].revents & POLLERR){
                printf("get an error from %d\n",fds[i].fd);
                char errors[100];
                memset(errors,'\0',100);
                socklen_t length = sizeof(errors);
                if(getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,&errors,&length) < 0){
                    perror("getsockopt()");
                }
                continue;
            }else if(fds[i].revents & POLLRDHUP){
                /*如果客户端关闭连接，则服务器也关闭对应连接，并将用户数减1*/
                Users[fds[i].fd] = Users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                i--;
                user_counter--;
                printf("a client left\n");
            }else if(fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(Users[connfd].buf,'\0',BUFFER_SIZE);
                ret = recv(connfd,Users[connfd].buf,BUFFER_SIZE-1,0);
                if(ret < 0){
                    /*如果操作出错，则关闭连接*/
                    if(errno != EAGAIN){
                        perror("recv()");
                        Users[fds[i].fd] = Users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        user_counter--;
                        i--;
                        close(connfd);                     
                    }
                }else if(ret == 0){}
                else{
                    /*如果接收到客户数据，则通知其他socket连接准备写数据*/
                    for(j = 1;j <= user_counter;++j){
                        if(fds[j].fd == connfd)
                            continue;
                        fds[j].events &= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        Users[fds[j].fd].write_buf = Users[connfd].buf;
                    }
                }
            }else if(fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if(!Users[connfd].write_buf){
                    continue;
                }
                ret = send(connfd,Users[connfd].write_buf,strlen(Users[connfd].write_buf),0);
                Users[connfd].write_buf = NULL;
                /*写完数据后要重新注册fds[i]上的可读事件*/
                fds[i].events &= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
            
        }
    }
    free(Users);
    close(listenfd);    
    exit(0);
}