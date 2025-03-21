#pragma once
#include <string>
#include <iostream>
#include <string.h>
#include <unordered_map>

class HTTP
{
public:
    std::string method;
    std::string url;
    std::string version;
    std::unordered_map<std::string, std::string> m_map;
    std::string ip;
    std::string port;
    std::string content;
    void prase(char* buf);
    bool prase_all();
    void prase_url();

    std::string http_code;
    std::string accept;

    std::string code_text;
    std::string charset;
    std::string text_l;
    bool keep_alive;

    std::string geturl() const { return url; }

    void return_res(char* res, std::string& filename);
private:

};