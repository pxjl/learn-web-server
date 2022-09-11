#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

#define MAX_EVENTS 1024
#define BUF_SIZE 1024

void errif(bool condition, const char *errmsg)
{
    if (condition)
    {
        perror(errmsg);
        exit(EXIT_FAILURE);
    }
}

void setnonblocking(int sock)
{
    int opts;
    opts = fcntl(sock, F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts | O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}

int main(int argc, char const *argv[])
{
    // server socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    errif(sockfd == -1, "sock creat error");

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8888);

    errif(bind(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) == -1, "sock bind error");

    errif(listen(sockfd, SOMAXCONN) == -1, "sock listen error");

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    bzero(&clnt_addr, sizeof(clnt_addr));

    char buf[BUF_SIZE];

    // epoll
    int epfd = epoll_create1(0);
    errif(epfd == -1, "epoll create error");
    epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = sockfd;
    setnonblocking(sockfd);
    epoll_ctl(epfd, EPOLLIN, sockfd, &event);
    while (true)
    {
        int count_fd = epoll_wait(epfd, events, MAX_EVENTS, -1);
        errif(count_fd == -1, "epoll error");
        for (int i = 0; i < count_fd; i++)
        {
            if (events[i].data.fd == sockfd)
            {
                int clnt_fd = accept(sockfd, (sockaddr *)&clnt_addr, &clnt_addr_len);
                event.data.fd = clnt_fd;
                event.events = EPOLLIN | EPOLLET;
                setnonblocking(clnt_fd);
                epoll_ctl(epfd, EPOLLIN, clnt_fd, &event);
                printf("new client fd %d! IP: %s Port: %d\n", clnt_fd, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
            }
            else if (events[i].events & EPOLLIN)
            {
                while (true)
                {
                    bzero(buf, sizeof(buf));
                    ssize_t read_size = read(events[i].data.fd, buf, sizeof(buf));
                    if (read_size > 0)
                    {
                        printf("message from client fd %d: %s\n", events[i].data.fd, buf);
                        write(events[i].data.fd, buf, sizeof(buf));
                    }
                    else if (read_size == -1 && errno == EINTR)
                    { //客户端正常中断、继续读取
                        continue;
                    }
                    else if (read_size == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
                    { //非阻塞IO，这个条件表示数据全部读取完毕
                        //该fd上数据读取完毕
                        break;
                    }
                    else if (read_size == 0)
                    {                             // EOF事件，一般表示客户端断开连接
                        close(events[i].data.fd); //关闭socket会自动将文件描述符从epoll树上移除
                        break;
                    }
                    else
                        break;
                }
            }
        }
    }
    close(sockfd);
    close(epfd);
    return 0;
}
