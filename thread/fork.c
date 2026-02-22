#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>

pthread_mutex_t mutex;

void *another(void *){
    printf("in child thread,lock the mutex\n");
    pthread_mutex_lock(&mutex);
    sleep(5);
    pthread_mutex_unlock(&mutex);
}

int main(){
    pthread_mutex_init(&mutex,NULL);
    pthread_t id;
    if(pthread_create(&id,NULL,another,NULL) != 0){
        perror("pthread_create()");
        exit(1);
    }
    sleep(1);
    pthread_t pid = fork();
    if(pid < 0){
        perror("fork()");
        pthread_join(id,NULL);
        pthread_mutex_destroy(&mutex);
        exit(1);
    }else if(pid == 0){
        printf("i am in the child,want to get the lock");
        pthread_mutex_lock(&mutex);
        printf("i can not run to here\n");
        pthread_mutex_unlock(&mutex);
        exit(0);
    }else{
        wait(NULL);
    }
    pthread_join(id,NULL);
    pthread_mutex_destroy(&mutex);
    exit(0);
}