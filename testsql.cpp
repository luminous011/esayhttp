#include <iostream>
#include <map>
#include "sqlconn.h"

using namespace std;

int main()
{
    string user = "debian-sys-maint";
    string passwd = "DHiZn6lRjee4emT0";
    string databasename = "yourdb";
    connection_pool* sql = connection_pool::GetInstance();
    sql->init("localhost", user, passwd, databasename, 3306);
    map<string, string> users = sql->getUser();
    for(auto i : users)
    {
        cout << "user " << i.first << " password " << i.second << endl;
    }
    sql->registerUser("daad", "9dadj");
    cout << endl;
    auto lll = sql->getMap();
    for(auto i : lll)
    {
        cout << "user " << i.first << " password " << i.second << endl;
    }
    return 0;
}