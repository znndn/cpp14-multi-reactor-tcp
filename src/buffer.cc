#include <string>
#include <vector>
#include "buffer.h"
#include <errno.h>
#include <sys/uio.h>
#include <algorithm>
#include <unistd.h>
#include <assert.h>

Buffer::Buffer()
{
    readerIndex=SpaceForProtHeader;
    writerIndex=SpaceForProtHeader;
    buffer.resize(SpaceForProtHeader+SpaceInitialize);
}

int Buffer::readableBytes()
{
    return writerIndex-readerIndex;
}
int Buffer::writableBytes()
{
    return buffer.size()-writerIndex;
}
int Buffer::beforeHeaderBytes()
{
    return readerIndex;
}

char* Buffer::MsgStartLoc()
{
    auto BufferStart=&buffer[0];
    return BufferStart + readerIndex;
}

void Buffer::useData(int UsedLength)
{
    if (UsedLength<readableBytes())
    {
        readerIndex+=UsedLength;
    }
    else
    {
        useAllData();
    }
}
void Buffer::useAllData()
{
    readerIndex=SpaceForProtHeader;
    writerIndex=SpaceForProtHeader;
}

void Buffer::ensureNewStringSize(int ByteRequested)
{
    if (writableBytes()>ByteRequested)
    {
    }
    else
    {
        MoveorExpend(ByteRequested);
    }
}

void Buffer::MoveorExpend(int size)
{
    assert(size>=0 && writerIndex<=buffer.size());

    if (buffer.size()-writerIndex < size)
    {
        buffer.resize(writerIndex + size);
    }
    else
    {
        std::copy(&buffer[0] + readerIndex,
                  &buffer[0] + writerIndex,
                  &buffer[0] + SpaceForProtHeader);
        writerIndex = writerIndex - readerIndex + SpaceForProtHeader;
        readerIndex = SpaceForProtHeader;
    }
}

char* Buffer::beginWriteLoc()
{
    return &buffer[0] + writerIndex;
}

void Buffer::bufferAppend(int size,const char* data)
{
    ensureNewStringSize(size);
    std::copy(data, data+size, beginWriteLoc());
    writerIndex += size;
}

int Buffer::readFd(int fd, int *saveErrno)
{
    // 单次 readv 可溢出到 64 KiB 栈空间，随后只追加实际读到的数据。
    char AuxBuffer[65536];

    struct iovec vec[2];
    vec[0].iov_base=&buffer[0]+writerIndex;
    vec[0].iov_len=buffer.size()-writerIndex;
    vec[1].iov_base=&AuxBuffer[0];
    vec[1].iov_len=65536;

    int iovcnt;
    int originalFreeSpace=writableBytes();
    if (originalFreeSpace<65536) iovcnt=2;
    else iovcnt=1;

    const struct iovec* iov=vec;
    int result=readv(fd, &vec[0], iovcnt);

    if (result==-1)
    {
        *saveErrno=errno;
    }
    else if (result<=originalFreeSpace)
    {
        writerIndex+=result;
    }
    else
    {
        // 成员缓冲区已填满，将备用区中的剩余字节追加到末尾。
        writerIndex=buffer.size();
        bufferAppend(result-originalFreeSpace,&AuxBuffer[0]);
    }
    return result;
}

int Buffer::writeFd(int fd, int *saveErrno)
{
    int result=::write(fd, MsgStartLoc(), readableBytes());
    if (result==-1)
    {
        *saveErrno=errno;
    }
    return result;
}

std::string Buffer::retrieveAsString(size_t len)
{
    std::string str;
    auto pointer=MsgStartLoc();
    for (int i=0;i<len;i++)
    {
        str.push_back(*pointer);
        pointer++;
    }
    useData(len);
    return str;
}

std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readableBytes());
}
