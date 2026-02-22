#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

int main(){

    const char *addr = "192.168.1.133";
    
    uint32_t conv_ddr = inet_addr(addr);
    printf("Network ordered integer addr:%#x\n",conv_ddr);

    exit(0);
}
