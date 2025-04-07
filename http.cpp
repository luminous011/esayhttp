#include "http.h"
#include <string>
#include <sstream>
#include <fstream>
#include "sqlconn.h"
#include <string.h>
#include <zlib.h>
#include <vector>
#include <nlohmann/json.hpp>
#include <sys/stat.h>

#define BUFSIZE 8192

const std::string is_user_true(const std::string& username, const std::string& password)
{
    connection_pool* sql = connection_pool::GetInstance();
    auto user_pass = sql->getMap();
    // for(auto i : user_pass)
    // {
    //     std::cout << "user: " << i.first << "pw: " << i.second << std::endl;
    // }
    auto it = user_pass.find(username);
    if(it == user_pass.end())
        return "user don't exists";
    if(it->second != password)
        return "password error";
    return "login success";
}

const std::string is_register_true(const std::string& username, const std::string& password)
{
    connection_pool* sql = connection_pool::GetInstance();
    auto user_pass = sql->getMap();
    auto it = user_pass.find(username);
    if(it != user_pass.end())
    {
        return "exists";
    }
    sql->registerUser(username, password);
    return "success";
}

void readHtmlFile(const std::string& filename, char*& buffer, size_t& size) 
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        buffer = nullptr;
        size = 0;
        return;
    }
    size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[size];
    file.read(buffer, size);
    file.close();
}

char* getLastNChars(const char* str, int n) 
{
    if (str == nullptr || n <= 0) {
        return nullptr;
    }
    int len = strlen(str);
    if (n > len) {
        n = len;
    }
    char* result = new char[n + 1];
    strncpy(result, str + len - n, n);
    result[n] = '\0';
    return result;
}

std::string hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int digest_len;

    HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
         digest, &digest_len);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }

    return oss.str();
}

const std::string secret_key = "abcdefgh";

std::string generateSignedCookie(const std::string& username) {
    time_t expire_time = time(nullptr) + 604800; // 7 天后过期
    std::string data = "user=" + username + "&expires=" + std::to_string(expire_time);
    std::string sign = hmac_sha256(data, secret_key);
    
    std::string cookie_value = data + "&sign=" + sign;
    
    return cookie_value + "; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800";
}

bool verifySignedCookie(const std::string& cookie) {
    size_t attr_pos = cookie.find(";");
    std::string cookie_value = (attr_pos == std::string::npos) ? cookie : cookie.substr(0, attr_pos);
    
    size_t sign_pos = cookie_value.find("&sign=");
    if (sign_pos == std::string::npos) {
        return false;
    }
    std::string data = cookie_value.substr(0, sign_pos);
    std::string provided_sign = cookie_value.substr(sign_pos + 6);
    
    std::string calculated_sign = hmac_sha256(data, secret_key);
    return calculated_sign == provided_sign;
}

// bool gzipCompress(const char* input, size_t inputSize, char*& output, size_t& outputSize) 
// {
//     z_stream strm;
//     strm.zalloc = Z_NULL;
//     strm.zfree = Z_NULL;
//     strm.opaque = Z_NULL;

//     if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
//         return false;
//     }

//     outputSize = compressBound(inputSize);
//     output = new char[outputSize];

//     strm.avail_in = static_cast<uInt>(inputSize);
//     strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input));
//     strm.avail_out = static_cast<uInt>(outputSize);
//     strm.next_out = reinterpret_cast<Bytef*>(output);

//     if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
//         deflateEnd(&strm);
//         delete[] output;
//         return false;
//     }

//     outputSize = strm.total_out;
//     deflateEnd(&strm);
//     return true;
// }

// bool gzipDecompress(const char* input, size_t inputSize, char*& output, size_t& outputSize) 
// {
//     z_stream strm;
//     strm.zalloc = Z_NULL;
//     strm.zfree = Z_NULL;
//     strm.opaque = Z_NULL;
//     strm.avail_in = static_cast<uInt>(inputSize);
//     strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input));

//     if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
//         return false;
//     }

//     // 初始时假设输出大小为输入大小的10倍，可根据实际情况调整
//     outputSize = inputSize * 10;
//     output = new char[outputSize];
//     strm.avail_out = static_cast<uInt>(outputSize);
//     strm.next_out = reinterpret_cast<Bytef*>(output);

//     int ret = inflate(&strm, Z_FINISH);
//     if (ret != Z_STREAM_END) {
//         inflateEnd(&strm);
//         delete[] output;
//         return false;
//     }

//     outputSize = strm.total_out;
//     inflateEnd(&strm);
//     return true;
// }
bool gzipCompress(const char* input, size_t inputSize, char*& output, size_t& outputSize) 
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        std::cerr << "gzipCompress: deflateInit2 failed" << std::endl;
        return false;
    }

    outputSize = compressBound(inputSize);
    output = new (std::nothrow) char[outputSize];
    if (output == nullptr) {
        std::cerr << "gzipCompress: memory allocation failed" << std::endl;
        deflateEnd(&strm);
        return false;
    }

    strm.avail_in = static_cast<uInt>(inputSize);
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input));
    strm.avail_out = static_cast<uInt>(outputSize);
    strm.next_out = reinterpret_cast<Bytef*>(output);

    if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        std::cerr << "gzipCompress: deflate failed" << std::endl;
        deflateEnd(&strm);
        delete[] output;
        return false;
    }

    outputSize = strm.total_out;
    deflateEnd(&strm);
    return true;
}

bool gzipDecompress(const char* input, size_t inputSize, char*& output, size_t& outputSize) 
{
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = static_cast<uInt>(inputSize);
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input));

    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        std::cerr << "gzipDecompress: inflateInit2 failed" << std::endl;
        return false;
    }

    // 初始时假设输出大小为输入大小的10倍，可根据实际情况调整
    outputSize = inputSize * 10;
    output = new (std::nothrow) char[outputSize];
    if (output == nullptr) {
        std::cerr << "gzipDecompress: memory allocation failed" << std::endl;
        inflateEnd(&strm);
        return false;
    }

    strm.avail_out = static_cast<uInt>(outputSize);
    strm.next_out = reinterpret_cast<Bytef*>(output);

    int ret = inflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        std::cerr << "gzipDecompress: inflate failed" << std::endl;
        inflateEnd(&strm);
        delete[] output;
        return false;
    }

    outputSize = strm.total_out;
    inflateEnd(&strm);
    return true;
}


void HTTP::prase(char* buf)
{
    if (buf == nullptr) {
        return;
    }

    char* token = strtok(buf, "\r\n");
    if (token != nullptr) {
        m_map["line"] = std::string(token);
        std::string line = std::string(token);
        std::istringstream iss(line);
        iss >> method >> url >> version;
    }

    token = strtok(nullptr, "\r\n");
    int i = 0;
    while (token != nullptr) {
        std::string headerLine = std::string(token);
        size_t colonPos = headerLine.find(": ");
        if (colonPos != std::string::npos) {
            std::string headerKey = headerLine.substr(0, colonPos);
            std::string headerValue = headerLine.substr(colonPos + 1);
            size_t start = headerValue.find_first_not_of(" \t");
            size_t end = headerValue.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                headerValue = headerValue.substr(start, end - start + 1);
            }
            m_map[headerKey] = headerValue;
        }
        else
        {
            content = headerLine;//something error
            break;
        }
        token = strtok(nullptr, "\r\n");
        i++;
    }
}

bool HTTP::is_upload()
{
    auto it = m_map.find("Content-Type");
    if(it != m_map.end())
    {
        string str = it->second;
        if(str.find("multipart/form-data")!=string::npos)
            return true;
    }
    return false;
}

bool HTTP::prase_all()
{
    auto it = m_map.find("Host");
    if(it == m_map.end())
        return false;
    size_t pos = it->second.find(':');
    if(pos != std::string::npos)
    {
        ip = it->second.substr(0, pos);
        port = it->second.substr(pos+1);
    }
    
    it = m_map.find("Connection");
    if(it == m_map.end())
        return false;
    alive = (it->second == "Keep-Alive" || it->second == "keep-alive") ? true : false;
    return true;
}

void HTTP::prase_url() {
    std::string decodedUrl;
    for (size_t i = 0; i < url.length(); ++i) {
        if(url[i] >= 'A' && url[i] <= 'Z')
            url[i] += 32;

        if (url[i] == '%' && i + 2 < url.length()) {
            
            std::string hexStr = url.substr(i + 1, 2);
            int hexValue;
            std::istringstream(hexStr) >> std::hex >> hexValue;
          
            decodedUrl += static_cast<char>(hexValue);
            i += 2; 
        } else {
            decodedUrl += url[i];
        }
    }
    this->url = decodedUrl;
}



void HTTP::return_res(char* buf, std::string& filename, bool keep_alive)
{
    is_cache = true;
    is_gzip = false;
    is_download = false;
    alive = false;
    text_len = 0;
    is_file = false;
    is_content = false;
    is_location = false;
    std::string location;
    is_cookie = false;
    std::string cookie;
    std::vector<char> buffer;
    url.erase(url.begin());

    bool need_auth = (public_routes.find(url) == public_routes.end());
    need_auth = (need_auth && method == "GET");

    auto it = m_map["Cookie"];
    std::cout << "cookie: " << it << std::endl;
    if(need_auth && !verifySignedCookie(it))
    {
        http_code = Code::Found;
        is_location = true;
        location = "/login.html";
        goto PRASE;
    }

    if((url == "" || url == "login.html") && verifySignedCookie(it))
    {   
        http_code = Code::Found;
        is_location = true;
        location = "/welcome.html";
        goto PRASE;
    }
    
    for (auto i : m_map)
    {
        if(i.first == "Connection")
        {
            alive = (i.second == "Keep-Alive" || i.second == "keep-alive") ? true : false;
            alive = (alive && keep_alive);
        }
        if(i.first == "Accept-Encoding")
        {
            if(i.second.find("gzip") != std::string::npos)
                is_gzip = true;
            else
                is_gzip = false;
        }
    }
    if(method == "GET")
    {   
        if(url == "")
        {
            url = "login.html";
            text_type = Type::HTML; 
        }
        if(url.find("favicon.ico") != std::string::npos)
        {
            url = "favicon.ico";
            is_cache = false;
            text_type = Type::PNG;
        }
        if(url.find("download") != std::string::npos)
        {
            url = "star.png";
            text_type = Type::PNG;
            is_download = true;
        } 
        if(url.find("mp4") != std::string::npos)
        {
            text_type = Type::VIDEO;
        }
        if(url.find("md") != std::string::npos)
        {
            text_type = Type::MARKDOWN;
        }
        filename = url;
        if(is_gzip && url == "login.html")
        {
            filename += ".gz";
            struct stat stat_buf;
            int rc = stat(filename.c_str(), &stat_buf);
            if (rc == 0) 
            {
                text_len = stat_buf.st_size;
                buffer.resize(text_len);
                http_code = Code::OK;
                is_file = true;
                is_content = false;
            } else 
            {
                http_code = Code::Not_Found;
                text_type = Type::PLAIN;
                is_content = true;
                is_file = false;
                text_len = sizeof("Your Request Error!!!") - 1; 
            }
        }
        else
        {
            struct stat stat_buf;
            int rc = stat(filename.c_str(), &stat_buf);
            if (rc == 0) 
            {
                text_len = stat_buf.st_size;
                buffer.resize(text_len);
                http_code = Code::OK;
                is_file = true;
                is_content = false;
            } else 
            {
                http_code = Code::Not_Found;
                text_type = Type::PLAIN;
                is_content = true;
                is_file = false;
                text_len = sizeof("Your Request Error!!!") - 1; 
            }
        }
        
        // std::ifstream file(url, std::ios::binary | std::ios::ate);
        // if (file) {
        //     buffer.clear();
        //     text_len= file.tellg(); 
        //     file.seekg(0, std::ios::beg);
        //     buffer.resize(text_len);
        //     file.read(buffer.data(), text_len); 
        //     file.close();
        //     http_code = Code::OK;
        //     is_file = true;
        //     is_content = false;
        // } else {
        //     http_code = Code::Not_Found;
        //     text_type = Type::PLAIN;
        //     is_content = true;
        //     is_file = false;
        //     text_len = sizeof("Your Request Error!!!") - 1; 
        // }
    }
    else if(method == "POST")
    {
        //文件上传为upload,另外进行处理
        std::cout << "url " << url << std::endl;
        if(url == "uploads")
        {
            //std::cout << "content " << content << endl;
            
            std::string response_header = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
            std::vector<char> response;
            response.insert(response.end(), response_header.begin(), response_header.end());
            std::copy(response.begin(), response.end(), buf);
            return;
        }
        else
        {
            //登录为json
            int end = content.find('}');
            content = content.substr(0, end+1);
            //url == login/register
            if (content.empty()) {
                std::cerr << "error, null json" << std::endl;
                return;
            }
            std::string username;
            std::string password;
            try {
                nlohmann::json j = nlohmann::json::parse(content);
                username = j["username"];
                password = j["password"];
                //std::cout << "user: " << username << std::endl;
                //std::cout << "pw: " << password << std::endl;
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "JSON error: " << e.what() << std::endl;
                return;
            }
            std::cout << "now url: " << url << std::endl;
            if(url == "login")
            {
                std::string res = is_user_true(username, password);
                std::cout << res << std::endl;
                if (res == "login success") 
                {
                    std::cout << "sucess login..........." << std::endl;
                    http_code = Code::Found;
                    is_content = false;
                    is_file = false;
                    text_len = 0;
                    is_location = true;
                    location = "/welcome.html";
                    is_cookie = true;
                    cookie = generateSignedCookie(username);
                }
                else 
                {
                    http_code = Code::Unauthorized;
                    is_content = true;
                    is_file = false;
                    text_type = Type::JSON;
                    const std::string error_msg = "{\"error\": \"Login failed\"}";
                    buffer.assign(error_msg.begin(), error_msg.end());
                    text_len = error_msg.size();
                }
            }
            else if(url == "register")
            {
                std::string res = is_register_true(username, password);
                //std::cout << res << std::endl;
                if(res == "success")//register
                {
                    http_code = Code::OK;
                    is_content = true;
                    is_file = false;
                    text_type = Type::HTML;
                    buffer.clear();
                    const std::string bstr = "Register success!";//something error in welcome.html
                    buffer.resize(bstr.size());
                    text_len = bstr.size();
                    std::copy(bstr.begin(), bstr.end(), buffer.begin());
                }
                else//user has exits
                {
                    http_code = Code::Conflict;
                    is_content = true;
                    is_file = false;
                    text_type = Type::HTML;
                    buffer.clear();
                    const std::string bstr = "Register success!";
                    text_len = bstr.size();
                    buffer.resize(bstr.size());
                    std::copy(bstr.begin(), bstr.end(), buffer.begin());
                }
            }
        }
            //else if(){} etc..
    }
    else if(method == "HEAD")
    {
        http_code = Code::OK;
        is_content = false;
        is_file = false;
    }
    else
    {
        is_file = false;
        is_content = true;
        http_code = Code::Method_Not_Allowed;
        text_type = Type::PLAIN;
        text_len = sizeof("Your Request Error!!!") - 1; 
    }

PRASE:
    std::string code_num;
    std::string code_text;
    process_code(http_code, code_num, code_text);
    //std::cout << "httpcode: " << code_num << " " << code_text << std::endl;

    // std::string content_type;
    // process_type(text_type, content_type);
    std::string response_header;
    //add line
    response_header += version + " " + code_num + " " + code_text + "\r\n";
    //add header
    if(!is_cache)
    {
        response_header += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
        response_header += "Expires: 0\r\n";
    }
    if(is_download)
    {
        filename = url;
        response_header += "Content-Disposition: attachment; filename=" + url + "\r\n";
    }

    if(text_type == Type::HTML)
    {
        response_header += "Content-Type: text/html; charset=utf-8\r\n";
    }
    else if(text_type == Type::JSON)
    {
        response_header += "Content-Type: application/json; charset=utf-8\r\n";
    }
    else if(text_type == Type::PLAIN)
    {
        response_header += "Content-Type: text/plain; charset=utf-8\r\n";
    }
    else if(text_type == Type::PNG)
    {
        response_header += "Content-Type: image/png\r\n";
    }
    else if(text_type == Type::VIDEO)
    {
        response_header += "Content-Type: video/mp4\r\n";
    }
    else if(text_type == Type::MARKDOWN)
    {
        response_header += "Content-Type: text/markdown\r\n";
    }
    
    if(alive)
    {
        response_header += "Connection: keep-alive\r\nKeep-Alive: timeout=30, max=100\r\n";
    }
    else
    {
        response_header += "Connection: Close\r\n";
    }
    if(is_gzip && url == "login.html")
    {
        response_header += "Content-Encoding: gzip\r\nVary: Accept-Encoding\r\n";
    }

    if(is_cookie)
    {
        response_header += "Set-Cookie: ";
        response_header += cookie;
        response_header += "\r\n";
    }   
    //todo gzip

    //add content
    
    if(is_file)
    {
        response_header += "Content-Length: ";
        response_header += std::to_string(text_len);
        response_header += "\r\n"; 
        response_header += "\r\n"; 
        std::copy(response_header.begin(), response_header.end(), buf);
    }
    else if(is_content)
    {
        response_header += "Content-Length: ";
        response_header += std::to_string(text_len);
        response_header += "\r\n"; 
        response_header += "\r\n"; 
        std::vector<char> response;
        response.insert(response.end(), response_header.begin(), response_header.end());
        response.insert(response.end(), buffer.begin(), buffer.end());
        std::copy(response.begin(), response.end(), buf);
    }
    else if(is_location)
    {
        response_header += "Content-Length: ";
        response_header += std::to_string(text_len);
        response_header += "\r\n"; 
        response_header += "Location: ";
        response_header += location;
        response_header += "\r\n\r\n";
        std::vector<char> response;
        response.insert(response.end(), response_header.begin(), response_header.end());
        std::copy(response.begin(), response.end(), buf);
    }
    else
    {
        const char* error_msg = "Your Request Error!!!";
        response_header += "Content-Length: ";
        response_header += std::to_string(strlen(error_msg));
        response_header += "\r\n\r\n";
        std::vector<char> response;
        response.insert(response.end(), response_header.begin(), response_header.end());
        response.insert(response.end(), error_msg, error_msg + strlen(error_msg));
        std::copy(response.begin(), response.end(), buf);
    }
}


// void HTTP::return_res(char* buf, std::string& filename) 
// {
//     bool is_have_content = false;
//     bool is_file_send = false;
//     bool is_need_cache = true;
//     bool is_location = false;
//     std::string location;
//     std::vector<char> buffer; 
//     std::string http_code;
//     std::string code_text;
//     size_t text_l = 0;

//     auto acceptEncodingIt = m_map.find("Accept-Encoding");
//     bool canGzip = acceptEncodingIt != m_map.end() && acceptEncodingIt->second.find("gzip") != std::string::npos;

//     if (method == "GET") 
//     {
//         auto it = m_map.find("Accept");
//         if (it == m_map.end()) 
//         {
//             http_code = "400";
//             code_text = "Bad Request";
//         }

//         url.erase(url.begin());
//         std::cout << url << std::endl;
//         if (url.empty())
//             url = "zhuye.html";
//         if(url.find("favicon.ico") != std::string::npos)
//             url = "favicon.png";
//         if(url.find("download") != std::string::npos)
//             url = "star.png";
//         filename = url;
//         std::ifstream file(url, std::ios::binary | std::ios::ate);
//         if (file) {
//             buffer.clear();
//             text_l = file.tellg(); 
//            file.seekg(0, std::ios::beg);
//             buffer.resize(text_l);
//             file.read(buffer.data(), text_l); 
//             file.close();

//             http_code = "200";
//             code_text = "OK";
//             is_have_content = true;
//             is_file_send = true;
//             if (url == "favicon.png" || url == "star.png") {
//                 is_need_cache = false;
//                 accept = "image/png";
//             } else {
//                 accept = "text/html";
//             }
//         } else {
//             http_code = "404";
//             code_text = "Not Found";
//             accept = "text/plain";
//             text_l = sizeof("Your Request Error!!!") - 1; 
//         }
//     } 
//     else if (method == "POST") 
//     {
//         int end = content.find('}');
//         content = content.substr(0, end+1);
//         url.erase(url.begin());//url == login/register
//         if (content.empty()) {
//             std::cerr << "错误：输入的 JSON 字符串为空，无法解析。" << std::endl;
//             return;
//         }
//         std::string username;
//         std::string password;
//         try {
//             nlohmann::json j = nlohmann::json::parse(content);
//             username = j["username"];
//             password = j["password"];
//             std::cout << "user: " << username << std::endl;
//             std::cout << "pw: " << password << std::endl;
//         } catch (const nlohmann::json::parse_error& e) {
//             std::cerr << "JSON 解析错误: " << e.what() << std::endl;
//             return;
//         }
//         外部调用 username 和 password 做登录验证
//         if(url == "login")
//         {
//             std::string res = is_user_true(username, password);
//             std::cout << res << std::endl;

//             if (res == "login success") 
//             {
//                 http_code = "302";
//                 code_text = "Found";
//                 is_have_content = false;
//                 is_file_send = false;
//                 text_l = 0;
//                 is_location = true;
//                 location = "/welcome.html";
//             }
//             else 
//             {
//                 http_code = "401";
//                 code_text = "Unauthorized";
//                 is_have_content = true;
//                 accept = "application/json";
//                 const std::string error_msg = "{\"error\": \"Login failed\"}";
//                 buffer.assign(error_msg.begin(), error_msg.end());
//                 text_l = error_msg.size();
//             }
//         }
//         if(url == "register")
//         {
//             std::string res = is_register_true(username, password);
//             std::cout << res << std::endl;
//             if(res == "success")//register
//             {
//                 http_code = "200";
//                 code_text = "OK";
//                 is_have_content = true;
//                 accept = "text/html";
//                 buffer.clear();
//                 const std::string bstr = "Register success!";//something error in welcome.html
//                 buffer.resize(bstr.size());
//                 text_l = bstr.size();
//                 std::copy(bstr.begin(), bstr.end(), buffer.begin());
//             }
//             else//user has exits
//             {
//                 http_code = "409";
//                 code_text = "Conflict";
//                 is_have_content = true;
//                 accept = "text/html";
//                 buffer.clear();
//                 const std::string bstr = "Register success!";
//                 text_l = bstr.size();
//                 buffer.resize(bstr.size());
//                 std::copy(bstr.begin(), bstr.end(), buffer.begin());
//             }
//         }   
//     } 
//     else if (method == "HEAD")
//     {
//         http_code = "200";
//         code_text = "OK";
//         is_have_content = false;
//     } 
//     else 
//     {
//         http_code = "405";
//         code_text = "Method Not Allowed";
//         accept = "text/plain";
//         text_l = sizeof("Your Request Error!!!") - 1; 
//     }
//     std::cout << http_code << " " << code_text << std::endl;
//     std::string response_header;
//     response_header += version;
//     response_header += " ";
//     response_header += http_code;
//     response_header += " ";
//     response_header += code_text;
//     response_header += "\r\n";
//     if(!is_need_cache)
//     {
//         response_header += "Cache-Control: no-cache, no-store, must-revalidate\r\n";
//         response_header += "Expires: 0\r\n";
//     }
//     if(url == "star.png")
//     {
        
//         response_header += "Content-Disposition: attachment; filename=star.png\r\n";
//     }
//     if (accept == "text/html") {
//         response_header += "Content-Type: text/html; charset=utf-8\r\n";
//     } else if (accept == "application/json") {
//         response_header += "Content-Type: application/json; charset=utf-8\r\n";
//     } else {
//         response_header += "Content-Type: ";
//         response_header += accept;
//         response_header += "\r\n";
//     }

//     response_header += "Connection: ";
//     response_header += (alive == true) ? "Keep-Alive" : "Close";
//     response_header += "\r\n";

//     if (canGzip && is_have_content) {
//         std::vector<char> compressedBuffer;
//         size_t compressedSize = 0;
//         char* tempCompressed = nullptr;

//         if (gzipCompress(buffer.data(), buffer.size(), tempCompressed, compressedSize)) {
//             std::cout << "Gzip 压缩成功，压缩前大小: " << buffer.size() << ", 压缩后大小: " << compressedSize << std::endl;
//             compressedBuffer.assign(tempCompressed, tempCompressed + compressedSize);
//             delete[] tempCompressed;

//             buffer = compressedBuffer;
//             text_l = compressedSize;
//             response_header += "Content-Encoding: gzip\r\n";
//         } else {
//             std::cerr << "Gzip 压缩失败，使用未压缩数据" << std::endl;
//         }
//     }

//     if(is_file_send)
//     {
//         response_header += "Content-Length: ";
//         response_header += std::to_string(text_l);
//         response_header += "\r\n\r\n"; 
//         std::copy(response_header.begin(), response_header.end(), buf);
//     }
//     else if(is_have_content)
//     {
//         response_header += "Content-Length: ";
//         response_header += std::to_string(text_l);
//         response_header += "\r\n\r\n";
//         std::vector<char> response;
//         response.insert(response.end(), response_header.begin(), response_header.end());
//         response.insert(response.end(), buffer.begin(), buffer.end());
//         std::copy(response.begin(), response.end(), buf);
//     }
//     else if(is_location)
//     {
//         response_header += "Location: ";
//         response_header += location;
//         response_header += "\r\n";
//         response_header += "Content-Length: ";
//         response_header += std::to_string(text_l);
//         response_header += "\r\n\r\n";
//         std::vector<char> response;
//         response.insert(response.end(), response_header.begin(), response_header.end());
//         std::copy(response.begin(), response.end(), buf);
//     }
//     else
//     {
//         const char* error_msg = "Your Request Error!!!";
//         response_header += "Content-Length: ";
//         response_header += std::to_string(strlen(error_msg));
//         response_header += "\r\n\r\n";
//         std::vector<char> response;
//         response.insert(response.end(), response_header.begin(), response_header.end());
//         response.insert(response.end(), error_msg, error_msg + strlen(error_msg));
//         std::copy(response.begin(), response.end(), buf);
//     }
    
    
// }    