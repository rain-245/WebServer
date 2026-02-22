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
    socklen_t cilent_len;
    int i,str_len;
    serv_sock = socket(AF_INET,SOCK_DGRAM,0);
    if(serv_sock < 0){
        perror("socket()");
        exit(1);
    }

    if(argc < 2){
        fprintf(stderr,"Usage : %s <port>\n",argv[0]);
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

    while (1)
    {
        cilent_len = sizeof(client_addr);
        str_len = recvfrom(serv_sock,buf,BUFSIZE,0,(void *)&client_addr,&cilent_len);
        printf("Received %d bytes from client\n", str_len);
        sendto(serv_sock,buf,str_len,0,(void *)&client_addr,cilent_len);
    }
    
    close(serv_sock);
    exit(0);
}