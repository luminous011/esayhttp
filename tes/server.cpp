#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define PORT 8080

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 创建套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置套接字选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 绑定套接字
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // 接受连接
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // 打开文件
    int file_fd = open("favicon.ico", O_RDONLY);
    if (file_fd < 0) {
        perror("无法打开文件");
        close(new_socket);
        return 1;
    }

    // 获取文件大小
    off_t fileSize = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    // 发送文件大小
    write(new_socket, &fileSize, sizeof(fileSize));

    // 发送文件内容
    char buffer[1024];
    ssize_t bytesRead, bytesSent;
    while ((bytesRead = read(file_fd, buffer, sizeof(buffer))) > 0) {
        bytesSent = write(new_socket, buffer, bytesRead);
        if (bytesSent < 0) {
            perror("发送文件失败");
            break;
        }
    }

    std::cout << "文件已发送" << std::endl;

    close(file_fd);
    close(new_socket);
    close(server_fd);
    return 0;
}