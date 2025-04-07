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

    void prase(char* buf);
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

    void return_res(char* res, std::string& filename, bool keep_alive);


private:

};
