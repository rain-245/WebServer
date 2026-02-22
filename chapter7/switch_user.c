#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>

static int switch_to_user(uid_t user_id,gid_t gp_id){

    /*先确保目标用户不是root*/
    if((user_id == 0) && (gp_id == 0)){
        return 0;
    }

    /*确保当前用户是合法用户：root或者目标用户*/
    gid_t gid = getgid();
    uid_t uid = getuid();
    if(((gid != 0) || (uid != 0)) && ((gid != gp_id) || (uid != user_id))){
        return 0;
    } 

    /*如果不是root,则已是目标用户*/
    if(uid != 0){
        return 1;
    }

    /*切换到目标用户*/
    if((setgid(gp_id) < 0) || setuid(user_id) < 0){
        return 0;
    }

    return 1;
}