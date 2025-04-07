#include <iostream>
#include <string>
using namespace std;
int main()
{
    std::string username;
    std::string password;
    std::string usernameKey = "username=";
        std::string passwordKey = "password=";
    string content = "username=test&password=1234";
        size_t usernamePos = content.find(usernameKey);
        if (usernamePos != std::string::npos) 
        {
            usernamePos += usernameKey.length();
            size_t usernameEndPos = content.find('&', usernamePos);
            if (usernameEndPos == std::string::npos) 
                username = content.substr(usernamePos);
            else 
                username = content.substr(usernamePos, usernameEndPos - usernamePos);
        }

        size_t passwordPos = content.find(passwordKey);
        if (passwordPos != std::string::npos) 
        {
            passwordPos += passwordKey.length();
            size_t passwordEndPos = content.find('&', passwordPos);
            if (passwordEndPos == std::string::npos) 
                password = content.substr(passwordPos);
            else
                password = content.substr(passwordPos, passwordEndPos - passwordPos);
        }

    cout << username << password;
    return 0;
}