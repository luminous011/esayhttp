#pragma once

#include <list>
#include <mysql/mysql.h>
#include <string.h>
#include <iostream>
#include <string>
#include <mutex>
#include <map>

using namespace std;

class connection_pool
{
public:
    MYSQL *GetConnection();
    bool ReleaseConnection(MYSQL *coon);

    static connection_pool *GetInstance();

    void init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int port);

    std::map<std::string, std::string> getMap() const { return user_pass; }
    void close() 
    { 
        if (con) {
            mysql_close(con);
        }
    }

    connection_pool();
    ~connection_pool();

    bool registerUser(const std::string& username, const std::string& password);
    std::map<std::string, std::string> getUser();


public:
    MYSQL* con;
    std::map<std::string, std::string> user_pass;
};



