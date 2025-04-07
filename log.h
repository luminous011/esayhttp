#pragma once

#include <iostream>
#include <fstream>
#include <time.h>
#include <string>
#include <mutex>

#define LOG(ip, port, httpreq, httpres, i) Log::getInstance()->log(ip, port, httpreq, httpres, i)

class Log
{
public:
    static Log* getInstance()
    {
        static Log u;
        return &u;
    }

    void log(const std::string ip, const std::string port, const char* httpreq, const char* httpres, int i)
    {
        time_t now = time(0);
        tm* localTime = localtime(&now);
        m_mtx.lock();
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);

        std::ofstream logFile(filename, std::ios::app);
        if (logFile.is_open()) {
            if(i == 0)//request
            {
                logFile << buffer << '\t' << ip << ": " << port << '\t' << "http request: " << httpreq << std::endl;
                logFile.close();
                m_mtx.unlock();
            }
            else//reponse
            {
                logFile << buffer << '\t' << ip << ": " << port << '\t' << "http response: " << httpres << std::endl;
                logFile.close();
                m_mtx.unlock();
            }
            //std::cout << "log success" << std::endl;
        }
        else {
            std::cerr << "Can't open: " << filename << std::endl;
            m_mtx.unlock();
        }
    }
    

    void setFilename(const std::string& file)
    {
        filename = file;
    }

private:
    std::string filename;
    std::mutex m_mtx;
};
    