#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#include<unistd.h>

pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;
int a = 0,b = 0;
void *fun(void *agrv){
    pthread_mutex_lock(&mutex_b);
    printf("int child thread,got mutex_b,waiting for mutex_a\n");
    sleep(5);
    ++b;
    pthread_mutex_lock(&mutex_a);
    b+=a++;
    pthread_mutex_unlock(&mutex_a);
    pthread_mutex_unlock(&mutex_b);
    pthread_exit(NULL);
}

int main(){
    pthread_mutex_init(&mutex_a,NULL);
    pthread_mutex_init(&mutex_b,NULL);

    pthread_t pid;
    pthread_create(&pid,NULL,fun,NULL);
    pthread_mutex_lock(&mutex_a);
    printf("in parent thread,got mutex_a,waiting for mutex_b\n");
    sleep(5);
    ++a;
    pthread_mutex_lock(&mutex_b);
    a+=b++;
    pthread_mutex_unlock(&mutex_b);
    pthread_mutex_unlock(&mutex_a);
    exit(0);
}