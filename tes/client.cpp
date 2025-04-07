#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

#define PORT 8080

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    // 创建套接字
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "套接字创建错误" << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 将IPv4地址从文本转换为二进制格式
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "无效地址/地址不受支持" << std::endl;
        return -1;
    }

    // 连接到服务器
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "连接失败" << std::endl;
        return -1;
    }

    // 接收文件大小
    off_t fileSize;
    read(sock, &fileSize, sizeof(fileSize));

    // 创建文件
    int file_fd = open("received_favicon.ico", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("无法创建文件");
        close(sock);
        return 1;
    }

    // 接收文件内容
    char buffer[1024];
    ssize_t bytesRead, bytesWritten;
    off_t totalBytesRead = 0;
    while (totalBytesRead < fileSize) {
        bytesRead = read(sock, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            perror("接收文件失败");
            break;
        }
        bytesWritten = write(file_fd, buffer, bytesRead);
        if (bytesWritten < 0) {
            perror("写入文件失败");
            break;
        }
        totalBytesRead += bytesWritten;
    }

    std::cout << "文件已接收并保存为 received_favicon.ico" << std::endl;

    close(file_fd);
    close(sock);
    return 0;
}