#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

void errif(bool condition, const char *errmsg)
{
    if (condition)
    {
        perror(errmsg);
        exit(EXIT_FAILURE);
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

    char buf[1024];

    // select
    fd_set reads, reads_copy;
    FD_ZERO(&reads);
    FD_SET(sockfd, &reads);
    int fd_max = sockfd;
    reads_copy = reads;

    timeval timeout;

    while (1)
    {
        reads = reads_copy;
        timeout.tv_sec = 5;
        timeout.tv_usec = 5000;
        int res = select(fd_max + 1, &reads, nullptr, nullptr, &timeout);
        if (res == -1)
            break;
        if (res == 0)
        {
            printf("wait 5 second\n");
            continue;
        }
        for (int i = 0; i < fd_max + 1; i++)
        {
            if (FD_ISSET(i, &reads))
            {
                if (i == sockfd)
                {
                    int clnt_sockfd = accept(sockfd, (sockaddr *)&clnt_addr, &clnt_addr_len);
                    printf("new client fd %d! IP: %s Port: %d\n", clnt_sockfd, inet_ntoa(clnt_addr.sin_addr), ntohs(clnt_addr.sin_port));
                    FD_SET(clnt_sockfd, &reads_copy);
                    fd_max = fd_max < clnt_sockfd ? clnt_sockfd : fd_max;
                }
                else
                {
                    bzero(buf, sizeof(buf));
                    ssize_t read_size = read(i, buf, sizeof(buf));
                    if (read_size > 0)
                    {
                        printf("message from client fd %d: %s\n", i, buf);
                        write(i, buf, sizeof(buf));
                    }
                    else
                    {
                        printf("client fd %d disconnected\n", i);
                        FD_CLR(i, &reads_copy);
                        close(i);
                    }
                }
            }
        }
    }
    close(sockfd);
    return 0;
}
