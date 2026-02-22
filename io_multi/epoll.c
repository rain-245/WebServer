#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#define MAX_EVENT_NUMBER 1024

/*poll和epoll在使用上的差别*/
int main() {
    // ------------------------------
    // poll 方式：索引就绪文件描述符
    // ------------------------------
    struct pollfd fds[MAX_EVENT_NUMBER];
    int ret = poll(fds, MAX_EVENT_NUMBER, -1);

    /* 必须遍历所有已注册文件描述符并找到其中的就绪者（可利用 ret 优化） */
    for (int i = 0; i < MAX_EVENT_NUMBER; ++i) {
        if (fds[i].revents & POLLIN) { /* 判断第 i 个文件描述符是否就绪 */
            int sockfd = fds[i].fd;
            /* 处理 sockfd */
        }
    }

    // ------------------------------
    // epoll 方式：索引就绪文件描述符
    // ------------------------------
    int epollfd = epoll_create(MAX_EVENT_NUMBER);
    struct epoll_event events[MAX_EVENT_NUMBER];
    ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);

    /* 仅遍历就绪的 ret 个文件描述符 */
    for (int i = 0; i < ret; ++i) {
        int sockfd = events[i].data.fd;
        /* sockfd 肯定就绪，直接处理 */
    }

    close(epollfd);
    return 0;
}