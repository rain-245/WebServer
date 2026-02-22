#include"processpool.h"
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>

class cgi_conn{
    private:
        /*读缓冲区的大小*/
        static const int BUFF_SIZE = 1024;
        static int m_epollfd;
        int m_sockfd;
        sockaddr_in client_addr;
        char m_buf[BUFF_SIZE];
        /*标记读缓冲中已经读入的客户数据的最后一个位置的下一个位置*/
        int m_read_idx;

    public:
        cgi_conn(){}
        ~cgi_conn(){}
        void init(int epollfd,int sockfd,const sockaddr_in &addr){
            m_epollfd = epollfd;
            m_sockfd = sockfd;
            client_addr = addr;
            memset(m_buf,'\0',sizeof(m_buf));
            m_read_idx = 0;
        }

        void process(){
            int idx = 0;
            int ret = -1;
            while(true){
                idx = m_read_idx;
                ret = recv(m_sockfd,m_buf + m_read_idx,BUFF_SIZE - 1 - m_read_idx,0);
                /*如果读操作发生错误，则关闭客户连接。但如果是暂时无数据可读，则退出循环*/
                if(ret < 0){
                    if(errno != EAGAIN)
                        remove_fd(m_epollfd,m_sockfd);
                    break;
                }else if(ret == 0){
                    /*如果对方关闭连接，则服务器也关闭连接*/
                    remove_fd(m_epollfd,m_sockfd);
                    break;
                }else{
                    m_read_idx += ret;
                    printf("user content is %s\n",m_buf);
                    for(;idx < m_read_idx;idx++){
                        /*防止数组越界,检查idx >= 1 HTTP 报文的结构要求每行必须以 \r\n结尾*/
                        if(m_buf[idx] == '\n' && m_buf[idx -1] == '\r' && idx > 0 ){
                            break;
                        }
                    }
                    /*如果没有读取到"/r/n",则需要读取更多客户数据*/
                    if(idx == m_read_idx){
                        continue;
                    }
                    m_buf[idx - 1] = '\0';

                    char *filename = m_buf;
                    /*判断客户要运行的CGI程序是否存在*/
                    if(access(filename,F_OK) == -1){
                        remove_fd(m_epollfd,m_sockfd);
                        break;
                    }
                    
                    /*创建子进程来执行CGI程序*/
                    pid_t pid = fork();
                    if(pid < 0){
                        remove_fd(m_epollfd,m_sockfd);
                        break;
                    }else if(pid == 0){
                        /*子进程将标准输出重定向到m_sockfd中,并执行CGI程序*/
                        close(STDOUT_FILENO);
                        dup(m_sockfd);
                        execl(m_buf,m_buf,NULL);
                        exit(0);
                    }else{
                        /*父进程只需关闭连接*/
                        remove_fd(m_epollfd,m_sockfd);
                        break;
                    }
                }
            }
        }
};
int cgi_conn::m_epollfd = -1;

int main(int argc, char **argv){
    if(argc < 3){
        fprintf(stderr,"Usage ip port\n");
        exit(-1);
    }

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd < 0){
        perror("socket()");
        exit(1);
    }

    sockaddr_in serv_addr;
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET,argv[1],&serv_addr.sin_addr);
    if(bind(listenfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(listenfd,5) < 0){
        perror("listen()");
        exit(1);
    }

    processpool<cgi_conn> *pool = processpool<cgi_conn>::create(listenfd);
    if(pool){
        pool->run();
        delete pool;
    }

    /*RAII思想,资源创建者回收自己创建的资源*/
    close(listenfd);
    exit(0);
}