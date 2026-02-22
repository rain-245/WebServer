#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<strings.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/sendfile.h>

int main(int argc,char **argv){

    int sockfd,conn,filefd;
    struct sockaddr_in serv_addr,client_addr;
    socklen_t client_len;
    struct stat stat_buf;

    const char *addr = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];
    
    if(argc < 3){
        fprintf(stderr,"Usage ip port filename\n");
        exit(1);
    }

    filefd = open(filename,O_RDONLY);
    if(filefd < 0){
        perror("open()");
        exit(1);
    }
    fstat(filefd,&stat_buf);

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        perror("socket()");
        exit(1);
    }

    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET,addr,&serv_addr.sin_addr);

    if(bind(sockfd,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(sockfd,5) < 0){
        perror("listen()");
        exit(1);
    }

    client_len = sizeof(client_addr);
    conn = accept(sockfd,(struct sockaddr *)&client_addr,&client_len);
    if(conn < 0){
        perror("accept()");
        exit(1);
    }else{
        sendfile(conn,filefd,NULL,stat_buf.st_size);
        close(conn);
    }

    close(sockfd);
    exit(0);
}