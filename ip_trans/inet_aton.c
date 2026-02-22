#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

int main(){

    const char *ip = "192.168.1.151";
    struct sockaddr_in addr_inet;

    inet_aton(ip,&addr_inet.sin_addr);
    printf("Network ordered integer addr:%#x\n",addr_inet.sin_addr.s_addr);
    exit(0);
}