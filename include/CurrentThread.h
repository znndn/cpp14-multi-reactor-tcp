#pragma once
#include <iostream>
#include <unistd.h>
#include <sys/syscall.h>

// 缓存 Linux 线程 ID，供 EventLoop 线程归属检查使用。
namespace CurrentThread
{
    extern __thread int threadID;

    void cacheThreadID();
    inline int GetTID()
    {
        if (__builtin_expect(threadID==0,0))
        {
            cacheThreadID();
        }
        return threadID;
    }
}
