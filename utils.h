#pragma once

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

struct ClientConnection {
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
    bool is_file_sending;

    struct sockaddr_in addr;
    
    // 上传文件相关字段
    enum class UploadState {
        NOT_UPLOADING,
        READING_HEADERS,
        READING_CONTENT
    } upload_state;
    
    std::string upload_boundary;
    std::string upload_filename;
    std::ofstream upload_file;
    size_t upload_content_length;
    size_t upload_received;
    size_t upload_file_offset;

    std::vector<char> upload_buffer;
    size_t header_parsed = 0;


    ClientConnection() 
    : fd(0),
      response_header_len(0),
      header_written(0),
      file_fd(-1),
      file_size(0),
      file_sent(0),
      is_header_sent(false),
      is_file_sending(false),
      upload_state(UploadState::NOT_UPLOADING),
      upload_content_length(0),
      upload_received(0),
      upload_file_offset(0) 
{
    memset(request, 0, BUFSIZE);
    memset(response_header, 0, BUFSIZE);
}

// 析构函数：确保资源释放
~ClientConnection() {
    reset();
    }

    void reset() {
        if (upload_file.is_open()) {
            upload_file.close();
        }
        if (file_fd != -1) {
            close(file_fd);
            file_fd = -1;
        }
        upload_state = UploadState::NOT_UPLOADING;
        upload_boundary.clear();
        upload_filename.clear();
        upload_content_length = 0;
        upload_received = 0;
        upload_file_offset = 0;
        upload_buffer.clear();
        memset(request, 0, BUFSIZE);
        is_header_sent = false;
        is_file_sending = false;
        filename.clear();
        if(upload_file.is_open())
        {
            upload_file.close();
            std::ofstream dummy;
            upload_file.swap(dummy);
        }
    }
};
void handle_request(ClientConnection& conn, bool keep_alive)
{
    HTTP http;
    memset(conn.response_header, 0, sizeof(conn.response_header));
    if(http.prase(conn.request, conn.fd) == false)
    {
        http.prase_all();
        http.prase_url();

        http.return_res(conn.response_header, conn.filename, keep_alive, conn.fd);
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
    else
    {
        http.return_res_ok(conn.response_header);
        conn.response_header_len = strlen(conn.response_header);
        conn.header_written = 0;
        conn.is_header_sent = false;
    }
    
}

static std::string sanitize_filename(const std::string& filename)
{
    std::string safe_name;
    for (char c : filename) {
        if (isalnum(c) || c == '.' || c == '_' || c == '-') {
            safe_name += c;
        } else {
            safe_name += '_';
        }
    }
    return safe_name.substr(0, 64); 
}

void process_upload_data(ClientConnection* conn, const char* data, size_t len, int epfd) {
    if(conn->upload_buffer.size() > 4 * BUFSIZE)
    {
        conn->reset();
        return;
    }

    if(len > BUFSIZE * 2)
    {
        conn->reset();
        return;
    }

    std::vector<char> full_data;
    if (!conn->upload_buffer.empty()) {
        full_data = conn->upload_buffer;
        conn->upload_buffer.clear();
    }
    full_data.insert(full_data.end(), data, data + len);

    const std::string boundary = "--" + conn->upload_boundary;
    const size_t boundary_len = boundary.size();
    size_t processed = 0;

    while (processed < full_data.size()) {
        if (conn->upload_state == ClientConnection::UploadState::READING_HEADERS) {
            const char* header_end = static_cast<const char*>(
                memmem(full_data.data() + processed, full_data.size() - processed,
                      "\r\n\r\n", 4)
            );
            
            if (header_end) {
                size_t header_size = header_end - (full_data.data() + processed) + 4;
                processed += header_size;
                conn->upload_state = ClientConnection::UploadState::READING_CONTENT;
            } else {
                conn->upload_buffer.assign(full_data.begin() + processed, full_data.end());
                return;
            }
        }

        if (conn->upload_state == ClientConnection::UploadState::READING_CONTENT) {
            const char* boundary_pos = static_cast<const char*>(
                memmem(full_data.data() + processed, full_data.size() - processed,
                      boundary.c_str(), boundary_len)
            );

            if (!boundary_pos) {
                size_t safe_write = full_data.size() - processed - boundary_len;
                if (safe_write > 0) {
                    conn->upload_file.write(full_data.data() + processed, safe_write);
                    conn->upload_received += safe_write;
                }
                conn->upload_buffer.assign(full_data.end() - boundary_len, full_data.end());
                break;
            }

            size_t data_size = boundary_pos - (full_data.data() + processed);
            conn->upload_file.write(full_data.data() + processed, data_size);
            conn->upload_received += data_size;
            processed += data_size + boundary_len;

            if (processed + 2 <= full_data.size() && 
                memcmp(full_data.data() + processed, "--", 2) == 0) {
                conn->upload_file.close();
                conn->upload_state = ClientConnection::UploadState::NOT_UPLOADING;
                std::cout << "file recv ..................." << std::endl;


                snprintf(conn->response_header, BUFSIZE,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 27\r\n\r\n"
                         "{\"status\":\"upload success\"}");
                conn->response_header_len = strlen(conn->response_header);
                conn->header_written = 0;
                conn->is_header_sent = false;

                struct epoll_event ev;
                ev.data.fd = conn->fd;
                ev.events = EPOLLOUT | EPOLLET;
                epoll_ctl(epfd, EPOLL_CTL_MOD, conn->fd, &ev);
                break;
            }
        }
    }
}


std::vector<char> extract_file_content(const std::vector<char> &raw_data,
                                       const std::string &boundary)
{
    std::vector<char> file_content;

    // 1. 将vector转换为string以便处理
    std::string data(raw_data.begin(), raw_data.end());

    // 2. 构建完整的边界标记
    std::string full_boundary = "--" + boundary;
    size_t boundary_pos = data.find(full_boundary);

    if (boundary_pos == std::string::npos)
    {
        return file_content; // 返回空vector表示未找到边界
    }

    // 3. 查找头部结束位置(第一个空行)
    size_t headers_end = data.find("\r\n\r\n", boundary_pos);
    if (headers_end == std::string::npos)
    {
        return file_content;
    }
    headers_end += 4; // 跳过空行

    // 4. 查找文件内容结束位置(下一个边界)
    size_t content_end = data.find(full_boundary, headers_end);
    if (content_end == std::string::npos)
    {
        // 如果没有找到结束边界，则取到数据末尾
        content_end = data.size();
    }

    // 5. 提取文件内容(去除末尾可能的\r\n)
    while (content_end > headers_end &&
           (data[content_end - 1] == '\n' || data[content_end - 1] == '\r'))
    {
        content_end--;
    }

    // 6. 拷贝文件内容到结果vector
    if (content_end > headers_end)
    {
        file_content.assign(data.begin() + headers_end,
                            data.begin() + content_end);
    }

    return file_content;
}

static const char* find_body_start(const char* data, ssize_t len)
{
    const char* marker = "\r\n\r\n"; // header与body分隔符
    const char* pos = static_cast<const char*>(memmem(data, len, marker, 4));
    return pos ? pos + 4 : nullptr;  // +4跳过换行符
}


