#include "log.h"

int main()
{
    Log::getInstance()->setFilename("log.txt");
    LOG("192.168.29.133", 8888, "GET / HTTP/1.1", "HTTP/1.1 200 OK");


    return 0;
}