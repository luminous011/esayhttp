#include "sqlconn.h"
#include <mysql/mysql.h>

connection_pool* connection_pool::GetInstance()
{
    static connection_pool v;
    return &v;
}

connection_pool::connection_pool() {
}

connection_pool::~connection_pool() {
    if (con) {
        mysql_close(con);
    }
    //std::cout << "delete database";
}

void connection_pool::init(std::string url, std::string User, std::string PassWord, std::string DataBaseName, int port)
{
    con = mysql_init(con);
    if(con == NULL)
    {
        std::cout << "link mysql error" << std::endl;
        return;
    }

    con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), port, NULL, 0);
    if(con == NULL)
    {
        std::cout << "connect to sql error" <<  std::endl;
        return;
    }



}

MYSQL* connection_pool::GetConnection()
{
    if(con == NULL)
    {
        std::cout << "GetConnector error" << std::endl;
        return NULL;
    }
    else
    {
        return con;
    }
}

bool connection_pool::registerUser(const std::string& username, const std::string& password)
{
    connection_pool* sql = connection_pool::GetInstance();
    MYSQL* con = sql->GetConnection();
    if(con == NULL)
        return false;

    std::string insert = "INSERT INTO user (username, passwd) VALUES ('" + username + "', '" + password + "')";

    if (mysql_query(con, insert.c_str())) 
    {
        std::cout << "Insert error: " << mysql_error(con) << std::endl;
        mysql_close(con);
        return false;
    }

    user_pass = sql->getMap();

    user_pass[username] = password; 

    return true;
}

std::map<std::string, std::string> connection_pool::getUser()
{
    connection_pool* sql = connection_pool::GetInstance();

    MYSQL* con = sql->GetConnection();
    if (mysql_query(con, "SELECT username,passwd FROM user"))
    {
        std::cout << "SELECT error" << std::endl;
        return {};
    }
    MYSQL_RES *result = mysql_store_result(con);
    int num_fields = mysql_num_fields(result);

    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        user_pass[temp1] = temp2;
    }
    return user_pass;
}

