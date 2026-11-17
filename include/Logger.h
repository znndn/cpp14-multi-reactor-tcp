#pragma once

#include <iostream>
#include "noncopyable.h"

class Logger: noncopyable
{
    private:

    Logger();
    ~Logger()=default;
    static thread_local int logLevel; // 避免不同线程覆盖当前日志等级。

    public:

    static Logger& getLogger();

    enum LogLevel
    {
        info,error,fatal,debug
    };

    void setLogLevel(int level);
    void log(std::string msg);

    Logger& operator= (const Logger& other) = delete;
    Logger& operator= (const Logger&&) = delete;

    Logger (const Logger& other) = delete;
    Logger (const Logger&&) = delete;
};

#define log_info(logMessage,...) \
    do  \
    {   \
        Logger& logLoc=Logger::getLogger();  \
        logLoc.setLogLevel(Logger::LogLevel::info);   \
        char buffer[1024]={0};  \
        snprintf(&buffer[0],1024,logMessage,##__VA_ARGS__);  \
        logLoc.log(buffer); \
    } while (0)

#define log_error(logMessage,...) \
    do  \
    {   \
        Logger& logLoc=Logger::getLogger();  \
        logLoc.setLogLevel(Logger::LogLevel::error);   \
        char buffer[1024]={0};  \
        snprintf(&buffer[0],1024,logMessage,##__VA_ARGS__);  \
        logLoc.log(buffer); \
    } while (0)

#define log_debug(logMessage,...) \
    do  \
    {   \
        Logger& logLoc=Logger::getLogger();  \
        logLoc.setLogLevel(Logger::LogLevel::debug);   \
        char buffer[1024]={0};  \
        snprintf(&buffer[0],1024,logMessage,##__VA_ARGS__);  \
        logLoc.log(buffer); \
    } while (0)

#define log_fatal(logMessage,...) \
    do  \
    {   \
        Logger& logLoc=Logger::getLogger();  \
        logLoc.setLogLevel(Logger::LogLevel::fatal);   \
        char buffer[1024]={0};  \
        snprintf(&buffer[0],1024,logMessage,##__VA_ARGS__);  \
        logLoc.log(buffer); \
        exit(-1); \
    } while (0)
