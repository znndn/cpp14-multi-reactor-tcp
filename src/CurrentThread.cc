#include <iostream>
#include <unistd.h>
#include <sys/syscall.h>
#include <CurrentThread.h>

namespace CurrentThread
{
    __thread int threadID;
    void cacheThreadID()
    {
        if (threadID==0)
        {
            int curr_thread_id=syscall(SYS_gettid);
            threadID = static_cast<pid_t>(curr_thread_id);
        }
    }
}
