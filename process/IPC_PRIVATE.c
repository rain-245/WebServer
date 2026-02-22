#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<unistd.h>
#include<sys/wait.h>

union semun{
    int val;
    struct semid_ds* buf;
    unsigned short int *array;
    struct seminfo *__buf;
};

/*op为-1时执行P操作，op为1时执行V操作*/
void pv(int sem_id,int op){
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = op;
    sem_b.sem_flg = SEM_UNDO;
    semop(sem_id,&sem_b,1);
}

int main(){
    union semun sem_un;
    sem_un.val = 1;
    int sem_id = semget(IPC_PRIVATE,1,0666);
    semctl(sem_id,0,SETVAL,sem_un);

    pid_t pid = fork();
    if(pid < 0){
        perror("fork()");
        exit(1);
    }else if(pid == 0){
        printf("children try to get binery sem\n");
        pv(sem_id,-1);
        printf("chidlren get the sem and would release after 5 seconds\n");
        sleep(5);
        pv(sem_id,1);
    }else{
        printf("parent try to get binery sem\n");
        pv(sem_id,-1);
        printf("parent get the sem and would release after 5 seconds\n");
        sleep(5);
        pv(sem_id,1);
    }
    waitpid(pid,NULL,0);
    semctl(sem_id,0,IPC_RMID);

    exit(0);
}