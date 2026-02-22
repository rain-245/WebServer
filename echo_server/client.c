#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>

#define BUFSIZE     1024    

int main(int argc,char **argv){

    int client_sock;
    struct sockaddr_in serv_addr;
    socklen_t serv_len;
    int len;
    char buf[BUFSIZE];

    if(argc < 3){
        fprintf(stderr,"Usage %s <ip> <port>\n",argv[0]);
        exit(1);
    }

    client_sock = socket(AF_INET,SOCK_STREAM,0);
    if(client_sock < 0){
        perror("socket()");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);

    serv_len = sizeof(serv_addr);
    if(connect(client_sock,(void *)&serv_addr,serv_len) < 0){
        perror("connect()");
        exit(1);
    }

    while(1){
        puts("Input message(Q to quit)");
        fgets(buf,BUFSIZE,stdin);

        if((!strcmp(buf,"Q\n")) || (!strcmp(buf,"q\n"))){
            break;
        }

        write(client_sock,buf,strlen(buf));
        
        len = read(client_sock,buf,BUFSIZE-1);
        buf[len] = '\0';
        printf("Message from server : %s",buf);

    }
    
    close(client_sock);
    exit(0);
}