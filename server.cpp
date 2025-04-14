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
#include "utils.h"

using namespace std;


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


    int  sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
    string passwd = "DHiZn6lRjee4emT0";
    string databasename = "yourdb";
    connection_pool* sql = connection_pool::GetInstance();
    sql->init("localhost", user, passwd, databasename, 3306);
    map<string, string> users = sql->getUser();

    ClientConnection connections[CONSIZE];
    //memset(connections, 0, sizeof(connections));
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
                        connections[j].reset();
                        connections[j].fd = connfd;
                        connections[j].addr = cliaddr;
                        connections[j].upload_state = ClientConnection::UploadState::NOT_UPLOADING; // 新增初始化
                        break;
                    }
                }
                std::cout << "accept a new client: " << ip_str << ": " << ntohs(cliaddr.sin_port) << std::endl;

                setNonblocking(connfd);
                ev.data.fd = connfd;
                ev.events = EPOLLIN | EPOLLOUT;
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            else if (events[i].events & EPOLLIN) // read
            {
                std::cout << "begin read" << std::endl;
                int fd = events[i].data.fd;
                if (fd < 0)
                    continue;

                ClientConnection *conn = nullptr;
                for (int j = 0; j < CONSIZE; j++)
                {
                    if (connections[j].fd == fd)
                    {
                        conn = &connections[j];
                        break;
                    }
                }
                if (!conn)
                {
                    close(fd);
                    continue;
                }

                char buffer[BUFSIZE];
                bool is_upload_request = false;

                while (true)
                {
                    ssize_t bytes_read = read(fd, buffer, BUFSIZE - 1);
                    if (bytes_read > 0)
                    {
                        // 检测是否为上传请求
                        if (conn->upload_state == ClientConnection::UploadState::NOT_UPLOADING)
                        {
                            if (strstr(buffer, "POST /uploads") && strstr(buffer, "multipart/form-data"))
                            {
                                is_upload_request = true;

                                // 初始化上传参数
                                conn->upload_content_length = get_content_length(buffer);
                                conn->upload_boundary = get_boundary(buffer);
                                conn->upload_filename = "uploads/" + sanitize_filename(get_upload_file_name(buffer));

                                // 创建目录
                                mkdir("uploads", 0755);

                                // 打开文件流
                                conn->upload_file.open(conn->upload_filename, std::ios::binary | std::ios::trunc);
                                if (!conn->upload_file)
                                {
                                    std::cerr << "无法打开文件: " << conn->upload_filename << std::endl;
                                    conn->reset();
                                    close(fd);
                                    break;
                                }

                                // 定位正文起始
                                const char *body_start = find_body_start(buffer, bytes_read);
                                if (body_start)
                                {
                                    size_t header_len = body_start - buffer;
                                    size_t data_size = bytes_read - header_len;
                                    conn->upload_file.write(body_start, data_size);
                                    conn->upload_received = data_size;
                                    conn->upload_state = ClientConnection::UploadState::READING_CONTENT;
                                }
                                else
                                {
                                    conn->upload_state = ClientConnection::UploadState::READING_HEADERS;
                                    conn->upload_buffer.assign(buffer, buffer + bytes_read);
                                }
                            }
                            else
                            {
                                // 处理普通请求
                                strncpy(conn->request, buffer, bytes_read);
                                handle_request(*conn, keep_alive);
                                char ip_str[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET, &(conn->addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                                LOG(ip_str, std::to_string(ntohs(conn->addr.sin_port)), conn->request, nullptr, 0);
                                struct epoll_event ev;
                                ev.data.fd = fd;
                                ev.events = EPOLLOUT | EPOLLET;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                            }
                        }
                        else if (is_upload_request)
                        {
                            // 处理上传数据块
                            process_upload_data(conn, buffer, bytes_read, epfd);
                            
                        }
                    }
                    else if (bytes_read == 0)
                    {
                        conn->reset();
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN)
                            break;
                        perror("read error");
                        conn->reset();
                        close(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        break;
                    }
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                int fd = events[i].data.fd;
                if (fd < 0)
                    continue;

                ClientConnection *conn = nullptr;
                for (int j = 0; j < CONSIZE; j++)
                {
                    if (connections[j].fd == fd)
                    {
                        conn = &connections[j];
                        // std::cout << "bind suc" << std::endl;
                        break;
                    }
                }

                if (conn)
                {
                    if (!conn->is_header_sent) // header
                    {
                        ssize_t has_w = write(fd, conn->response_header + conn->header_written, conn->response_header_len - conn->header_written);
                        if (has_w > 0)
                        {
                            conn->header_written += has_w;
                            if (conn->header_written == conn->response_header_len)
                            {
                                // LOG(conn->ip, conn->port, nullptr, conn->response_header, 1);
                                conn->is_header_sent = true;
                                char ip_str[INET_ADDRSTRLEN];
                                std::cout << "write" << std::endl;
                                std::cout << conn->response_header << std::endl;
                                std::cout << "write end......." << std::endl;

                                inet_ntop(AF_INET, &(conn->addr.sin_addr), ip_str, INET_ADDRSTRLEN);
                                LOG(ip_str, std::to_string(ntohs(cliaddr.sin_port)), nullptr, conn->response_header, 1);
                            }
                        }
                        else if (has_w < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
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
                    if (conn->is_header_sent && conn->is_file_sending)
                    {
                        off_t offset = conn->file_sent;
                        ssize_t sent = sendfile(fd, conn->file_fd, &offset, conn->file_size - conn->file_sent);
                        if (sent > 0)
                        {
                            conn->file_sent += sent;
                            if (conn->file_sent == conn->file_size)
                            {
                                conn->is_file_sending = false;
                                close(conn->file_fd);
                                std::cout << "file send success: " << conn->filename << std::endl;
                                conn->reset();
                                ev.data.fd = fd;
                                ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                                epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
                            }
                        }
                        else if (sent < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
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
                    } // send success;
                }
                else
                {
                    close(fd);
                    events[i].data.fd = -1;
                    for (int j = 0; j < CONSIZE; j++)
                    {
                        if (connections[j].fd == fd)
                        {
                            connections[j].fd = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    for (auto &conn : connections)
        conn.reset();
    delete[] buf;
    close(epfd);

    return 0;
}
