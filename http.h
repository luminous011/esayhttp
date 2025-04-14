#pragma once
#include <string>
#include <iostream>
#include <string.h>
#include <unordered_map>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <set>

#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <fstream>
class HTTP
{
public:
    enum class Code
    {
        OK,
        Method_Not_Allowed,
        Conflict,
        Unauthorized,
        Found,
        Not_Found,
        Bad_Request
    };

    enum class Type
    {
        JSON,
        PNG,
        HTML,
        PLAIN,
        VIDEO,
        MARKDOWN,
    };


    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> m_map;
    std::string ip;
    std::string port;
    std::string content;
    //upload
    
    //upload

    bool prase(char* buf, int fd);
    bool prase_all();
    void prase_url();
    bool is_upload();
    

    Code http_code;
    void process_code(Code code, std::string& num, std::string& text)
    {
        switch (code)
        {
            case Code::OK:
            {
                num = "200";
                text = "OK";
                break;
            }
            case Code::Bad_Request:
            {
                num = "400";
                text = "Bad Request";
                break;
            }
            case Code::Conflict:
            {   
                num = "409";
                text = "Conflict";
                break;
            }
            case Code::Unauthorized:
            {
                num = "401";
                text = "Unauthorized";
                break;
            }
            case Code::Found:
            {
                num = "302";
                text = "Found";
                break;
            }
            case Code::Method_Not_Allowed:
            {
                num = "405";
                text = "Method Not Allowed";
                break;
            }
            case Code::Not_Found:
            {
                num = "404";
                text = "Not Fount";
                break;
            }
            
        }
    }
    Type text_type;
    bool alive;
    bool is_gzip;
    bool is_cache;
    bool is_download;
    bool is_file;
    bool is_content;
    bool is_location;
    bool is_cookie;
    size_t text_len;
    std::string charset;
    const std::set<std::string> public_routes = 
    {
        "",
        "login.html",
        "favicon.ico",
        "register"
    };
    std::string geturl() const { return url; }

    void return_res(char* res, std::string& filename, bool keep_alive, int fd);
    void return_res_ok(char* buf);


private:

};



// 从请求头中提取 Content-Length 的值
inline int get_content_length(const char *buf)
{
    std::string req(buf);
    size_t c_pos = req.find("Content-Length: ");
    if (c_pos == std::string::npos)
    {
        return -1; // 未找到 Content-Length 字段
    }
    c_pos += 16; // 移动到数字部分
    size_t e_pos = req.find("\r\n", c_pos);
    if (e_pos == std::string::npos)
    {
        return -1; // 未找到结束符
    }
    std::string length_str = req.substr(c_pos, e_pos - c_pos);
    try
    {
        return std::stoi(length_str);
    }
    catch (const std::invalid_argument &e)
    {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
    }
    catch (const std::out_of_range &e)
    {
        std::cerr << "Out of range: " << e.what() << std::endl;
    }
    return -1;
}

inline std::string get_upload_file_name(const char* buf) {
    std::string request(buf);
    
    // 1. 查找Content-Disposition头部
    size_t disp_pos = request.find("\r\nContent-Disposition:");
    if (disp_pos == std::string::npos) {
        std::cerr << "Content-Disposition header not found" << std::endl;
        return "";
    }

    // 2. 在Content-Disposition中查找filename
    size_t name_pos = request.find("filename=\"", disp_pos);
    if (name_pos == std::string::npos) {
        std::cerr << "Filename not found in Content-Disposition" << std::endl;
        return "";
    }
    name_pos += 10; // 跳过"filename=\""

    // 3. 查找文件名结束位置
    size_t end_pos = request.find('\"', name_pos);
    if (end_pos == std::string::npos) {
        std::cerr << "Invalid filename format" << std::endl;
        return "";
    }

    // 4. 提取文件名
    std::string filename = request.substr(name_pos, end_pos - name_pos);
    
    // 5. 安全处理：移除路径信息
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        filename = filename.substr(last_slash + 1);
    }

    // 6. 验证文件名
    if (filename.empty()) {
        std::cerr << "Empty filename" << std::endl;
        return "";
    }

    std::cout << "Parsed filename: " << filename << std::endl;
    return filename;
}
inline std::string get_file_content(const char* buf) {
    std::string req(buf);

    // 1. 查找文件部分的起始位置
    size_t disposition_pos = req.find("Content-Disposition: form-data;");
    if(disposition_pos == std::string::npos) {
        std::cerr << "Failed to find Content-Disposition." << std::endl;
        return "";
    }

    // 2. 查找Content-Type行(如果有)
    size_t content_type_pos = req.find("Content-Type:", disposition_pos);
    if(content_type_pos != std::string::npos) {
        // 跳过Content-Type行
        disposition_pos = req.find("\r\n\r\n", content_type_pos);
        if(disposition_pos == std::string::npos) {
            disposition_pos = content_type_pos;
        } else {
            disposition_pos += 4; // 跳过\r\n\r\n
        }
    } else {
        // 直接查找数据开始位置
        disposition_pos = req.find("\r\n\r\n", disposition_pos);
        if(disposition_pos == std::string::npos) {
            std::cerr << "Failed to find file data start." << std::endl;
            return "";
        }
        disposition_pos += 4; // 跳过\r\n\r\n
    }

    // 3. 查找文件数据的结束位置
    size_t boundary_pos = req.find("--", disposition_pos);
    if(boundary_pos == std::string::npos) {
        boundary_pos = req.length();
    }

    // 4. 提取文件数据
    std::string file_content = req.substr(disposition_pos, boundary_pos - disposition_pos);

    // 移除末尾的\r\n
    while(!file_content.empty() && 
          (file_content.back() == '\n' || file_content.back() == '\r')) {
        file_content.pop_back();
    }

    return file_content;
}

inline std::string get_boundary(const char* buf) {
    std::string req;
    req.append(buf);
    // 1. 查找 "Content-Type: multipart/form-data; boundary=..."
    size_t content_type_pos = req.find("Content-Type: multipart/form-data");
    if (content_type_pos == std::string::npos) {
        return ""; // 不是 multipart/form-data 请求
    }

    // 2. 查找 boundary= 的位置
    size_t boundary_pos = req.find("boundary=", content_type_pos);
    if (boundary_pos == std::string::npos) {
        return ""; // 未找到 boundary
    }
    boundary_pos += 9; // 跳过 "boundary="

    // 3. 提取 boundary 值（到行尾或分号结束）
    size_t boundary_end = req.find_first_of("\r\n;", boundary_pos);
    if (boundary_end == std::string::npos) {
        boundary_end = req.length(); // 如果未找到结束符，取到字符串末尾
    }

    std::string boundary = req.substr(boundary_pos, boundary_end - boundary_pos);

    // 移除可能的引号（如 boundary="----WebKitFormBoundary..."）
    if (!boundary.empty() && boundary.front() == '"') {
        boundary.erase(0, 1);
    }
    if (!boundary.empty() && boundary.back() == '"') {
        boundary.pop_back();
    }

    return boundary;
}

inline bool save_large_file(int fd, const std::string &boundary, const std::string &filename,
                     int content_size, const std::string &initial_content)
{
    // 以二进制模式打开文件
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file " << filename << ": " << strerror(errno) << std::endl;
        return false;
    }

    // 写入初始内容
    file.write(initial_content.c_str(), initial_content.size());
    if (file.fail())
    {
        std::cerr << "Failed to write initial content to file" << std::endl;
        file.close();
        return false;
    }

    int total_received = initial_content.size();
    char buffer[4096];
    std::string boundary_marker = "--" + boundary;

    while (total_received < content_size)
    {
        int bytes_read = recv(fd, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0)
        {
            if (bytes_read == 0)
            {
                std::cerr << "Connection closed by client" << std::endl;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                std::cerr << "recv error: " << strerror(errno) << std::endl;
            }
            break;
        }

        std::string chunk(buffer, bytes_read);
        size_t boundary_pos = chunk.find(boundary_marker);

        if (boundary_pos != std::string::npos)
        {
            // 写入边界前的数据
            file.write(chunk.c_str(), boundary_pos);
            total_received += boundary_pos;
            break;
        }
        else
        {
            file.write(chunk.c_str(), chunk.size());
            total_received += chunk.size();
        }

        if (file.fail())
        {
            std::cerr << "Failed to write chunk to file" << std::endl;
            file.close();
            return false;
        }
    }

    file.close();

    if (total_received < content_size)
    {
        std::cerr << "Incomplete file received. Expected: " << content_size
                  << ", Received: " << total_received << std::endl;
        // 删除不完整的文件
        unlink(filename.c_str());
        return false;
    }

    return true;
}


inline std::vector<char> read_file_with_boundary(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("无法打开文件: " + filename);
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        throw std::runtime_error("读取文件失败: " + filename);
    }
    
    return buffer;
}


inline std::vector<char> remove_boundary(std::vector<char>& file_data, const std::string& boundary) {
    std::string full_boundary = boundary;
    std::string data(file_data.begin(), file_data.end());
    
    // 查找第一个边界位置
    size_t first_boundary = data.find(full_boundary);
    if (first_boundary == std::string::npos) {
        throw std::runtime_error("未找到边界标记");
    }
    
    // 查找头部结束位置
    size_t headers_end = data.find("\r\n\r\n", first_boundary);
    if (headers_end == std::string::npos) {
        throw std::runtime_error("无效的文件格式");
    }
    headers_end += 4; // 跳过空行
    
    // 查找文件内容结束位置
    size_t last_boundary = data.rfind(full_boundary);
    if (last_boundary == std::string::npos) {
        throw std::runtime_error("未找到结束边界");
    }
    
    // 提取纯文件内容
    size_t content_length = last_boundary - headers_end;
    
    // 去除末尾的换行符
    while (content_length > 0 && 
          (data[headers_end + content_length - 1] == '\n' || 
           data[headers_end + content_length - 1] == '\r')) {
        content_length--;
    }
    
    std::vector<char> result(content_length);
    std::copy(file_data.begin() + headers_end, 
              file_data.begin() + headers_end + content_length, 
              result.begin());
    
    return result;
}