#define _GNU_SOURCE 
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>

int main(int argc,char **argv){

    if(argc < 2){
        fprintf(stderr,"Usage : filename\n");
        exit(1);
    }

    int filefd,pipefd_stdout[2],pipefd_file[2];
    filefd = open(argv[1],O_RDWR|O_TRUNC|O_CREAT,0666);
    if(filefd < 0){
        perror("open()");
        exit(1);
    }

    if(pipe(pipefd_stdout) < 0){
        perror("pipe()");
        exit(1);
    }

    if(pipe(pipefd_file) < 0){
        perror("pipe()");
        exit(1);
    }

    if(splice(STDIN_FILENO,NULL,pipefd_stdout[1],NULL,32768,SPLICE_F_MOVE|SPLICE_F_MORE) < 0){
        perror("splice()");
        exit(1);
    }

    if(tee(pipefd_stdout[0],pipefd_file[1],32768,SPLICE_F_NONBLOCK) < 0){
        perror("tee()");
        exit(1);
    }

    if(splice(pipefd_file[0],NULL,filefd,NULL,32768,SPLICE_F_MOVE|SPLICE_F_MORE) < 0){
        perror("splice()");
        exit(1);
    }

    if(splice(pipefd_stdout[0],NULL,STDOUT_FILENO,NULL,32768,SPLICE_F_MOVE|SPLICE_F_MORE) < 0){
        perror("splice()");
        exit(1);
    }

    close(filefd);
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);
    close(pipefd_file[0]);
    close(pipefd_file[1]);
    exit(0);
}