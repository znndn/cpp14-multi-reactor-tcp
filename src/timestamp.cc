#include <iostream>
#include <time.h>
#include <chrono>
#include <timestamp.h>

Timestamp::Timestamp()
{
    SecondsSinceEpoch_=0;
}

Timestamp::Timestamp(long SecondsSinceEpoch)
    :SecondsSinceEpoch_(SecondsSinceEpoch)
{
}

Timestamp Timestamp::now()
{
    Timestamp curr(time(NULL));
    return curr;
}

std::string Timestamp::toString() const
{
    char arr[128] = {0};
    time_t seconds = static_cast<time_t>(SecondsSinceEpoch_);
    struct tm curr = {};
    // 使用调用方提供的存储，避免多线程共享时间转换结果。
    if (::localtime_r(&seconds, &curr) == nullptr)
    {
        return "";
    }

    snprintf(arr, sizeof(arr), "%4d/%02d/%02d %02d:%02d:%02d",
             curr.tm_year + 1900, curr.tm_mon + 1, curr.tm_mday,
             curr.tm_hour, curr.tm_min, curr.tm_sec);

    return std::string(arr);
}
