# C++14 Multi-Reactor TCP Network Library

基于 C++14 的 Linux TCP 网络库，参考 [Muduo](https://github.com/chenshuo/muduo) 的 Reactor 架构实现。使用非阻塞 Socket、epoll LT 与 I/O 线程池，提供连接管理、缓冲读写、跨线程任务投递及应用回调接口。

## 核心实现

- **主从 Reactor**：主线程的 `Acceptor` 接收连接，`EventLoopThreadPool` 轮询选择 I/O 线程；每个线程运行一个 `EventLoop`，串行处理所属连接的读写事件。
- **任务投递**：`runInLoop` 在所属线程直接执行，跨线程调用转入任务队列；`queueInLoop` 始终入队，通过互斥锁保护队列、通过 `eventfd` 唤醒事件循环。
- **缓冲读写**：连接分别持有输入与输出 `Buffer`。接收端通过 `readv` 使用现有可写区与 64 KiB 栈上备用区；发送端先尝试直接写入，剩余数据交由输出缓冲区和可写事件继续发送。
- **生命周期管理**：`Socket` 负责关闭 fd，`unique_ptr` 管理内部资源；连接由 `shared_ptr` 持有，`Channel::tie` 使用 `weak_ptr` 在事件分发期间取得临时强引用。
- **连接回收与停机**：连接表在主线程维护，Channel 在所属 I/O 线程注销；主线程收到清理完成通知后，再回收 I/O 线程池。

## 模块关系

```text
baseLoop / 主线程
  Acceptor ── 接收连接 ──> TcpServer / 连接表
                              │ 轮询分配
                    EventLoopThreadPool
                       ├─ I/O 线程 1：EventLoop → EPollPoller → Channel
                       ├─ I/O 线程 2：EventLoop → EPollPoller → Channel
                       └─ ...                                  │
                                                       TcpConnection
                                                        Buffer / Socket
```

| 模块 | 职责 |
| --- | --- |
| `EventLoop`、`Channel`、`EPollPoller` | 事件循环、事件分发与 epoll 登记 |
| `Thread`、`EventLoopThread`、`EventLoopThreadPool` | 线程启动、事件循环发布与连接分配 |
| `Acceptor`、`TcpServer` | 监听接入、连接表和停机协调 |
| `TcpConnection`、`Buffer` | 连接状态、收发缓冲与应用回调 |
| `Socket`、`InetAddress` | fd 管理和 IPv4 地址封装 |

## 构建与运行

依赖 Linux、支持 C++14 的 GCC/Clang、CMake 3.16.3 或更高版本，以及 POSIX 线程库。

```bash
git clone https://github.com/znndn/cpp14-multi-reactor-tcp.git
cd cpp14-multi-reactor-tcp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/testserver
```

构建产生共享库 `build/src/libNetworkLib.so` 和回声示例 `build/testserver`。当前 CMake 配置默认启用 AddressSanitizer。

示例监听 `127.0.0.1:8080`，启动 10 个额外 I/O 线程。在同一台机器的另一终端连接：

```bash
nc 127.0.0.1 8080
```

输入文本即可收到原样回显。详细操作见 [回声示例](example/README.md)。

## 使用约定

- `TcpServer` 的配置、启动及停止在 `baseLoop` 所属线程调用。线程数为 0 时，由 `baseLoop` 同时处理接入和连接 I/O。
- `MessageCallback` 接收字节流；`Buffer` 保留未消费的数据，具体消息边界和协议拆包由应用层处理。回声示例不包含协议解析器。
- `TcpConnection::send` 可跨线程投递；同一连接的 I/O 在所属 loop 中执行。`shutdown` 等待输出缓冲排空后半关闭写端。
- `TcpServer::stop` 停止接受新连接，并等待现有连接结束。它没有强制断连或停机超时；应在完成回调中退出 `baseLoop`，之后再析构服务器。
- 示例在启动时忽略 `SIGPIPE`，使对端断开后的写错误进入库的错误处理路径。应用需要自行安排信号策略。

当前实现面向 IPv4 TCP。回声演示和自动构建用于检查基本使用与编译情况，不代表吞吐量、延迟或高并发性能结论。
