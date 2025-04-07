#include "http.h"
#include <string>
#include "sqlconn.h"
#include <stdio.h>
#include <zlib.h>
using namespace std;
int main()
{
    HTTP http;
    char buf[4096];
    // string user = "debian-sys-maint";
    // string passwd = "DHiZn6lRjee4emT0";
    // string databasename = "yourdb";
    // connection_pool* sql = connection_pool::GetInstance();
    // sql->init("localhost", user, passwd, databasename, 3306);
    // map<string, string> users = sql->getUser();
    const char* constBuf = "GET /download HTTP/1.1\r\nHost: example.com\r\nUser-Agent: Mozilla/5.0\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 27\r\nConnection: Keep-Alive\r\n\r\n";
    strcpy(buf, constBuf);
    http.prase(buf);
    http.prase_all();
    http.prase_url();
    char* buffer = new char[4096];
    char* token = strtok(buffer, "\r'n");
    std::string filename;
    http.return_res(buffer, filename);
    for(int i = 0; i < 4096; i++)
    {
        if(buffer[i] == '\0')
            break;
        printf("%c", buffer[i]);
    }

    return 0;
}