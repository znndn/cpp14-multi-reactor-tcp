#pragma once

#include <iostream>
#include <string>
#include <vector>

constexpr int SpaceForProtHeader = 8;
constexpr int SpaceInitialize = 1024;

// 可增长的字节缓冲区；消息边界和协议拆包由应用层处理。
class Buffer
{
    private:
    int readerIndex; // 可读区起点。
    int writerIndex; // 可写区起点。
    std::vector<char> buffer;
    void ensureNewStringSize(int ByteRequested);

    public:
    Buffer();
    ~Buffer(){};

    int readableBytes();
    int writableBytes();
    int beforeHeaderBytes();

    char* MsgStartLoc();

    void useData(int UsedLength);
    void useAllData();

    void MoveorExpend(int size);
    char* beginWriteLoc();
    void bufferAppend(int size, const char* data);

    // readv 使用现有可写区及栈上备用区，按实际读入量追加数据。
    int readFd(int fd, int *saveErrno);
    // 返回实际写入量；调用方负责消费相应字节。
    int writeFd(int fd, int *saveErrno);

    std::string retrieveAllAsString();
    // 调用方保证 len 不超过 readableBytes()。
    std::string retrieveAsString(size_t len);
};
