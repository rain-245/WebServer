#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<unistd.h>

#define BUFSIZE 1024

int main(int argc,char **argv){

    char buf[BUFSIZE];
    int serv_sock,client_sock;
    struct sockaddr_in serv_addr,client_addr;
    socklen_t cilent_len = sizeof(client_addr);
    int i,str_len;
    serv_sock = socket(AF_INET,SOCK_STREAM,0);

    if(argc < 2){
        fprintf(stderr,"Usage : %s <port>\n",argv[0]);
        exit(1);
    }

    if(serv_sock < 0){
        perror("socket()");
        exit(1);
    }

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(serv_sock,(void *)&serv_addr,sizeof(serv_addr)) < 0){
        perror("bind()");
        exit(1);
    }

    if(listen(serv_sock,5) < 0){
        perror("listen()");
        exit(1);
    }

    for(i = 0;i < 5;i++){
    client_sock = accept(serv_sock,(void *)&client_addr,&cilent_len);
    if(client_sock < 0){
        perror("accept()");
        exit(1);
    }else{
        printf("Connect client %d\n",i+1);
    }

    while((str_len = read(client_sock,buf,BUFSIZE)) != 0){
        write(client_sock,buf,str_len);
    }

    close(client_sock);
    }

    close(serv_sock);
    exit(0);
}