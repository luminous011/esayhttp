#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <assert.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <atomic>
#include <csignal>
#include <vector>

#include "log.h"
#include "sqlconn.h"
#include "http.h"

using namespace std;

#define BUFSIZE 8192
#define CONSIZE 256

enum class STATUS{
    prase_request = 0,
    prase_header = 1,
    prase_text = 2,
    prase_done = 3,
};

class HttpMethod
{
    enum class METHOD
    {
        GET = 0,
        POST = 1,
        HEAD = 2,
    };
};

std::atomic<bool> keep_alive(true);

void handle_signal(int sig)
{
    if(sig == SIGUSR1)
    {
        keep_alive = !keep_alive;
        std::string mode = (keep_alive)?("Keep_alive") : ("Close");
        std::cout << "switch mode to "
        << mode << std::endl;
    }
}

void setNonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

struct ClientConnection
{
    int fd;
    char request[BUFSIZE];
    char response_header[BUFSIZE];
    size_t response_header_len;
    size_t header_written;
    std::string filename;
    int file_fd;
    off_t file_size;
    off_t file_sent;

    bool is_header_sent;
    bool is_file_sending;//send file

    struct sockaddr_in addr;
    //upload file
    std::vector<char> dynamic_buf;
    std::string upload_filename;
    FILE* upload_file = nullptr;
    size_t content_length = 0;
    size_t received = 0;
};

void handle_request(ClientConnection& conn, bool keep_alive)
{
    HTTP http;
    memset(conn.response_header, 0, sizeof(conn.response_header));
    http.prase(conn.request);
    http.prase_all();
    http.prase_url();

    http.return_res(conn.response_header, conn.filename, keep_alive);
    conn.response_header_len = strlen(conn.response_header);
    conn.header_written = 0;
    conn.is_header_sent = false;
    if(conn.filename == "")
        return;
    conn.file_fd = open(conn.filename.c_str(), O_RDONLY);
    if(conn.file_fd >= 0)
    {
        struct stat file_stat;
        fstat(conn.file_fd, &file_stat);
        conn.file_size = file_stat.st_size;
        conn.file_sent = 0;
        conn.is_file_sending = true;
    }
    else
    {
        conn.is_file_sending = false;
        std::cout << "cannot open file" << conn.filename << std::endl;
    }
}
// void readHtmlFile(const std::string& filename, char* buffer) {
//     std::ifstream file(filename, std::ios::binary);
//     if (!file) {
//         std::cerr << "无法打开文件: " << filename << std::endl;
//         return;
//     }

//     file.read(buffer, BUFSIZE - 1);
//     if (file.gcount() > 0) {
//         buffer[file.gcount()] = '\0'; 
//     }
//     file.close();
// }

int main()
{
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) 
    {
        std::cout << "Failed to set signal handler" << std::endl;
        exit(EXIT_FAILURE);
    }


    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        std::cout << "Setsockopt failed" << std::endl;
        close(sockfd);
        return 0;
    }
    char* buf = new char[BUFSIZE];
    struct sockaddr_in seraddr, cliaddr;
    socklen_t clilen;
    int connfd;
    struct epoll_event ev, events[512];

    setNonblocking(sockfd);

    int epfd = epoll_create(CONSIZE);

    ev.data.fd = sockfd;
    ev.events = EPOLLIN | EPOLLOUT;

    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

    
    memset(&seraddr, 0, sizeof(seraddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(39333);

    if(bind(sockfd, (struct sockaddr*)&seraddr, sizeof(seraddr)) < 0)
    {
        std::cout << "bind error:" << errno << std::endl;
        close(sockfd);
        return 0;
    }

    if(listen(sockfd, 1024) < 0)
    {
        std::cout << "listen error" << std::endl;
        close(sockfd);
        return 0;
    }
    std::cout << "listen......." << std::endl;

    string user = "debian-sys-maint";
    string passwd = "2uKHg0CLgDPDTsN7";
    string databasename = "yourdb";
    connection_pool* sql = connection_pool::GetInstance();
    sql->init("localhost", user, passwd, databasename, 3306);
    map<string, string> users = sql->getUser();

    ClientConnection connections[CONSIZE];
    memset(connections, 0, sizeof(connections));
    Log::getInstance()->setFilename("log.txt");
    while(true)
    {
        int nfds = epoll_wait(epfd, events, CONSIZE, -1);
        if(nfds <= 0)
            continue;
        for(int i = 0; i < nfds; i++)
        {
            if(events[i].data.fd == sockfd)//accept
            {
                clilen = sizeof(cliaddr);
                memset(&cliaddr, 0, clilen); 
                if((connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
                {
                    std::cout << "accpet error" << std::endl;
                    return 0;
                }
                int sndbuf_size = 1 * 1024 * 1024;  //增加发送缓冲区大小
                setsockopt(connfd, SOL_SOCKET, SO_SNDBUF, &sndbuf_size, sizeof(sndbuf_size));
                char ip_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(cliaddr.sin_addr), ip_str, INET_ADDRSTRLEN);

                for (int j = 0; j < CONSIZE; j++) {
                    if (connections[j].fd == 0) {
                        connections[j].fd = connfd;
                        connections[j].addr = cliaddr;
                        break;
                    }
                }
                std::cout << "accept a new client: " << ip_str << ": " << ntohs(cliaddr.sin_port) << std::endl;

                setNonblocking(connfd);
                ev.data.fd = connfd;
                ev.events = EPOLLIN | EPOLLOUT;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            else if(events[i].events & EPOLLIN)//read
            {
                std::cout << "begin read" << std::endl;
                int fd = events[i].data.fd;
                if(fd < 0)
                    continue;
                memset(buf, 0, BUFSIZE);
                ssize_t has_read = read(fd, buf, BUFSIZE-1);
                buf[BUFSIZ-1] = '\0';
                if(has_read <= 0)
                {
                    close(fd);
                    events[i].data.fd = -1;
                    for(int j = 0; j < CONSIZE; j++)
                    {
                        if(connections[j].fd == fd)
                        {
                            connections[j].fd = 0;//delete error fd
                            break;
                        }
                    }
                }
                else
                {
                    buf[has_read] = '\0';
                    std::cout << "read: " << std::endl;
                    std::cout << buf << std::endl;
                    std::cout << "read end....... " << std::endl;

                    ev.data.fd = fd;
                    ev.events = EPOLLOUT | EPOLLET;
                    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                    int j = 0;
                    for(j ; j < CONSIZE; j++)//store this fd's read
                    {
                        if(connections[j].fd == fd)
                        {
                            std::cout << "begin prase" << std::endl;
                            strncpy(connections[j].request, buf, has_read);
                            char ip_str[INET_ADDRSTRLEN];
                            inet_ntop(AF_INET, &(connections[j].addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                            LOG(ip_str, std::to_string(ntohs(cliaddr.sin_port)), buf, nullptr, 0);
                            handle_request(connections[j], keep_alive); // prase the request and write the response to response
                            break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                int fd = events[i].data.fd;
                if(fd < 0)
                    continue;
                
                ClientConnection* conn = nullptr;
                for(int j = 0; j < CONSIZE; j++)
                {
                    if(connections[j].fd == fd)
                    {
                        conn = &connections[j];
                        //std::cout << "bind suc" << std::endl;
                        break;
                    }
                }
                
                if(conn)
                {
                    if(!conn->is_header_sent)//header
                    {
                        ssize_t has_w = write(fd, conn->response_header + conn->header_written, conn->response_header_len - conn->header_written);
                        if(has_w > 0)
                        {
                            conn->header_written += has_w;
                            if(conn->header_written == conn->response_header_len)
                            {
                                //LOG(conn->ip, conn->port, nullptr, conn->response_header, 1);
                                conn->is_header_sent = true;
                                char ip_str[INET_ADDRSTRLEN];
                                std::cout << "write" << std::endl;
                                std::cout << conn->response_header << std::endl;
                                std::cout << "write end......." << std::endl;

                                inet_ntop(AF_INET, &(conn->addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                                LOG(ip_str, std::to_string(ntohs(cliaddr.sin_port)), nullptr, conn->response_header, 1);
                            }
                        }
                        else if(has_w < 0)
                        {
                            if(errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                continue;
                            }
                            else
                            {
                                close(fd);
                                conn->fd = 0;
                            }
                        }
                    }
                    if(conn->is_header_sent && conn->is_file_sending)
                    {
                        off_t offset = conn->file_sent;
                        ssize_t sent = sendfile(fd, conn->file_fd, &offset, conn->file_size - conn->file_sent);
                        if(sent > 0)
                        {
                            conn->file_sent += sent;
                            if(conn->file_sent == conn->file_size)
                            {
                                conn->is_file_sending = false;
                                close(conn->file_fd);
                                std::cout << "file send success: " << conn->filename << std::endl;
                               
                                ev.data.fd = fd;
                                ev.events = EPOLLIN | EPOLLET;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                            }
                        }
                        else if(sent < 0)
                        {
                            if(errno == EAGAIN || errno == EWOULDBLOCK)
                            {
                                ev.events = EPOLLOUT | EPOLLET;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                                continue;
                            }
                            else
                            {
                                close(fd);
                                conn->fd = 0;
                                close(conn->file_fd);
                            }
                        }
                    }//send success;


                }
                else
                {
                    close(fd);
                    events[i].data.fd = -1;
                    for(int j = 0; j < CONSIZE; j++)
                    {
                        if(connections[j].fd == fd)
                        {
                            connections[j].fd = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    delete buf;
    close(epfd);

    return 0;
}
