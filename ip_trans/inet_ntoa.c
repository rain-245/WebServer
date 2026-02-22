#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
int main(){

    char arr[20];
    char *addr = NULL;
    struct sockaddr_in addr1;
    addr1.sin_addr.s_addr = htonl(0x1020304);
    
    addr = inet_ntoa(addr1.sin_addr);
    strcpy(arr,addr);

    puts(arr);

    exit(0);
}
