#include <string>
#include <iostream>

// 从 multipart/form-data 请求中提取文件正文内容（适用于任意文件类型）
std::string get_file_content(const char* buf) {
    std::string req(buf);

    // 1. 查找文件部分的起始位置（`Content-Disposition: form-data; name="file"; filename="..."`）
    size_t disposition_pos = req.find("Content-Disposition: form-data;");
    if (disposition_pos == std::string::npos) {
        std::cerr << "Failed to find Content-Disposition." << std::endl;
        return "";
    }

    // 2. 查找 `\r\n\r\n`，即文件数据的起始位置
    size_t file_start_pos = req.find("\r\n\r\n", disposition_pos);
    if (file_start_pos == std::string::npos) {
        std::cerr << "Failed to find file data start." << std::endl;
        return "";
    }

    file_start_pos += 4; // 跳过 `\r\n\r\n`

    // 3. 查找文件数据的结束位置（下一个 boundary 或字符串末尾）
    size_t boundary_pos = req.find("--", file_start_pos);
    size_t file_end_pos = (boundary_pos != std::string::npos) ? boundary_pos : req.length();

    // 4. 提取文件数据（移除末尾可能的 `\r\n`）
    std::string file_content = req.substr(file_start_pos, file_end_pos - file_start_pos);

    // 移除末尾的 `\r\n`（如果存在）
    while (!file_content.empty() && 
           (file_content.back() == '\n' || file_content.back() == '\r')) {
        file_content.pop_back();
    }

    return file_content;
}


inline std::string get_upload_file_name(const char* buf) {
    std::string req(buf);
    size_t start = req.find("filename=\"");
    if (start == std::string::npos) {
        std::cerr << "Filename not found in request" << std::endl;
        return "";
    }
    start += 10; // Skip "filename=\""
    
    size_t end = req.find("\"", start);
    if (end == std::string::npos) {
        std::cerr << "Invalid filename format" << std::endl;
        return "";
    }
    
    std::string filename = req.substr(start, end - start);
    
    // // Basic security check - remove path components
    // size_t slash_pos = filename.find_last_of("/\\");
    // if (slash_pos != std::string::npos) {
    //     filename = filename.substr(slash_pos + 1);
    // }
    
    return filename;
}

int main() {
    // 示例请求（上传一个文本文件）
    const char* buf = "POST /uploads HTTP/1.1\r\n"
                      "Host: 192.168.29.133:39333\r\n"
                      "Connection: keep-alive\r\n"
                      "Content-Length: 256\r\n"
                      "Content-Type: multipart/form-data; boundary=----WebKitFormBoundaryABC123\r\n"
                      "\r\n"
                      "------WebKitFormBoundaryABC123\r\n"
                      "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
                      "Content-Type: text/plain\r\n"
                      "\r\n"
                      "Hello, this is a test file!\r\n"
                      "Second line.";

    std::string file_content = get_upload_file_name(buf);
    if (!file_content.empty()) {
        std::cout << "Extracted file content:\n" << file_content << std::endl;
    } else {
        std::cerr << "Failed to extract file content." << std::endl;
    }

    return 0;
}