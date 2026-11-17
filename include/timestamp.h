#pragma once
#include <iostream>
#include <time.h>
#include <chrono>

// 秒级时间戳，用于事件接收时间和日志。
class Timestamp
{
    public:
    Timestamp();
    explicit Timestamp(long SecondsSinceEpoch);
    static Timestamp now();
    std::string toString() const;

    private:
    long SecondsSinceEpoch_;
};
