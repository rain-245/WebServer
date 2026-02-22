#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include<stdbool.h>
#include<sys/stat.h>
#include<fcntl.h>
bool daemonize(){

    pid_t pid = fork();
    if(pid < 0){
        perror("fork");
        exit(1);
    }else if(pid > 0){
        exit(0);
    }

    umask(0);
    pid_t sid = setsid();
    if(sid < 0){
        return false;
    }

    if(chdir("/") < 0){
        return false;
    }

    /*关闭标准输入设备，标准输出设备，标准错误输出设备*/
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /*将标准输入，标准输出，标准错误重定向到/dev/null文件*/
    open("/dev/null",O_RDONLY);
    open("/dev/null",O_RDWR);
    open("/dev/null",O_RDWR);
    
    return true;
    
}