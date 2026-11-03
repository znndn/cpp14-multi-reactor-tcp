#include <iostream>
#include <mutex>
#include "noncopyable.h"
#include "Logger.h"
#include "timestamp.h"

thread_local int Logger::logLevel = Logger::info;

Logger::Logger()
{
    logLevel=0;
}

Logger& Logger::getLogger()
{
    static Logger newLogger;
    return newLogger;
}

void Logger::setLogLevel(int level)
{
    logLevel=level;
}

void Logger::log(std::string msg)
{
    // 同一条日志的输出由共享互斥锁保护。
    static std::mutex outputMutex;
    std::lock_guard<std::mutex> lock(outputMutex);

    std::string prefVar;
    switch (logLevel)
    {
    case 0:
        prefVar="info ";
        break;
    case 1:
        prefVar="error ";
        break;
    case 2:
        prefVar="fatal ";
        break;
    case 3:
        prefVar="debug";
        break;
    default:
        break;
    }
    std::cout<<prefVar<<Timestamp::now().toString()<<" : "<<msg<<std::endl;
}
