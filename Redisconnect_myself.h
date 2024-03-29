//
// Created by LZH on 2023/6/28.
//

/**#ifndef REDISCONNECT_REDISCONNECT_MYSELF_H 是条件预处理指令，用于检查第一个名为 REDISCONNECT_REDISCONNECT_MYSELF_H
 * 的宏是否被定义，如果没有被定义过，也就是第一次包含该头文件，那么下面直到#endif之间的代码将会被编译，且只编译一次
 *
 * #ifndef #endif以外的代码不受限制，可以被多次编译
 **/
#ifndef REDISCONNECT_REDISCONNECT_MYSELF_H
#define REDISCONNECT_REDISCONNECT_MYSELF_H

#include "ResPool.h"

/**判断是否linux平台？**/
#ifdef LINUX

#include "errno.h"
#include "netdb.h"
#include "fcntl.h"
#include "signal.h"
#include "sys/time.h"
#include "sys/wait.h"
#include "sys/stat.h"
#include "sys/file.h"
#include "sys/types.h"
#include "sys/ioctl.h"
#include "arpa/inet.h"
#include "sys/epoll.h"
#include "sys/statfs.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "sys/syscall.h"




/**?**/
#define ioctlsocket ioctl
/**表示无效的socket**/
#define INVALID_SOCKET (SOCKET)(-1)

typedef int SOCKET;
#endif

class RedisConnect
{
    typedef std::mutex Mutex;
    typedef std::lock_guard<std::mutex> Locker;

    friend class Command;

public:
    static const int OK = 1;      /**成功**/
    static const int FAIL = -1;   /**失败**/
    static const int IOERR = -2;  /**IO错误**/
    static const int SYSERR = -3; /***系统错误**/
    static const int NETERR = -4; /**网络错误？**/
    static const int TIMEOUT = -5; /**超过时限**/
    static const int DATAERR = -6;/**日期错误？**/
    static const int SYSBUSY = -7;/**系统繁忙**/
    static const int PARAMERR = -8; /**参数错误**/
    static const int NOTFUND = -9;
    static const int NETCLOSE = -10; /**网络关闭**/
    static const int NETDELAY = -11; /**网络延迟**/
    static const int AUTHFAIL = -12; /**认证失败**/

public:
    static int POOL_MAXLEN;
    static int SOCKET_TIMEOUT;
public:
    class Socket{
    protected:
        SOCKET sock = INVALID_SOCKET;

    public:
        /**用于检查socket是否处于超时状态**/
        static bool IsSocketTimeout(){
#ifdef LINUX
            /** ==0 表示没有错误
             * EAGAIN 和 EWOULDBLOCK 在大多数情况下是等价的，表示操作被阻塞，但不是由于错误，可以稍后尝试
             * EINTR 表示操作被信号中断。当进程收到信号时，正在进行的系统调用可能会被中断
             * 以上表示当前操作无法完成或已被中断**/
            return errno == 0 || errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#else
            return WSAGetlastError() == WSAETIMEDOUT;
#endif
        }
        /**SocketClose 函数接受一个套接字 sock 作为参数，并首先调用 IsSocketClosed 函数检查套接字是否已经关闭。
         * 如果套接字已经关闭，函数直接返回，不做任何操作。否则，它会根据操作系统是 Linux 还是 Windows 执行相应的关闭操作**/
        static void SocketClose(SOCKET sock){
            if (IsSocketClosed(sock)) return;
#ifdef LINUX
            ::close(sock);
#else
            ::closesocket(sock);
#endif
        }
        /**IsSocketClosed 函数接受一个套接字 sock 作为参数，用于检查套接字是否已关闭。**/
        static bool IsSocketClosed(SOCKET sock){
            return sock == INVALID_SOCKET || sock < 0;
        }
        /**设置套接字（SOCKET）的发送超时时间
         * SOCKET sock：要设置超时的套接字。
         * int timeout：超时时间，以毫秒为单位。**/
        static bool SocketSetSendTimeout(SOCKET sock,int timeout){
#ifdef LINUX
            /**struct timeval 结构体，用于表示时间
             * tv_sec 表示秒数部分，tv_usec 表示微秒部分。
             * 将传入的 timeout 转换为秒和微秒，并设置到 tv 结构体中。
             * **/
            struct timeval tv;
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = timeout % 1000 * 1000;
            /**setsockopt 函数用于设置套接字（Socket）的选项，允许程序员控制套接字的行为。原型如下
             * int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
                1）sockfd ：socket文件描述符，用于标识要设置选项的套接字。
                2）level：选项所属的协议层或套接字类型，通常是 SOL_SOCKET 表示通用套接字选项，也可以是其他协议的特定选项。
                         常见的协议层包括 SOL_SOCKET（通用套接字选项）、IPPROTO_TCP（TCP 协议选项）、IPPROTO_UDP（UDP 协议选项）等。
                3）optname: 要设置的选项名称，表示需要修改的具体选项。例如，SO_REUSEADDR 表示允许地址重用，SO_RCVBUF 表示接收缓冲区大小等。
                4) optval：指向包含选项值的缓冲区的指针。这个缓冲区通常包含一个特定类型的数据，用于设置选项的值。
                5) optlen：optval 缓冲区的大小，以字节为单位。

             * 将套接字的发送超时设置为 tv 结构体中指定的时间。这样，当调用套接字的发送操作，
             * 如果发送操作在 timeout 毫秒内未能成功完成，就会返回一个超时错误。
             * sock  要设置的socket
             * SO_SNDTIMEO 是要设置的选项名称，它表示发送超时选项，用于设置发送数据时的超时限制。
             * &tv 是一个指向 timeval 结构体的指针,表示要设置的时间
             * sizeof 要设置的时间的大小**/
            return setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(char *)(&tv),sizeof(tv)) == 0;
#else
            return setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(char *)(&timeout),sizeof(timeout)) == 0;
#endif
        }
        /**和上面的差不多，上面是发送超时时间，这个是接收超时时间**/
        static bool SocketSetRecvTimeout(SOCKET sock,int timeout){
#ifdef LINUX
            struct timeval tv;
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = timeout % 1000 * 1000;

            return setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char *)(&tv),sizeof(tv)) == 0;
#else
            return setsockopt(sock,SOL_SOCKET, SO_RCVTIMEO, (char*)(&timeout), sizeof(timeout)) == 0;
#endif
        }



        /**创建一个socket，设置为非阻塞模式，然后尝试连接到指定的 IP 地址和端口。如果连接成功，
         * 将socket设置回阻塞模式，并返回套接字。，否则返回 INVALID_SOCKET 表示连接失败。**/
        SOCKET SocketConnectTimeout(const char * ip,int port,int timeout){
            u_long mode = 1;
            /**struct sockaddr_in {
                short sin_family;      // 地址家族，通常为 AF_INET（IPv4）
                unsigned short sin_port;  // 端口号，以网络字节序表示（大端字节序）
                struct in_addr sin_addr;  // IPv4 地址
                char sin_zero[8];       // 未使用，通常用 0 填充
            };
            **/
            struct sockaddr_in addr;
            /**socket() 函数是用于创建套接字（socket）的系统调用，它用于在网络编程中创建一个新的通信端点。
             * 原型如下 int socket(int domain, int type, int protocol);
             * 1）domain 参数指定了套接字的地址家族（Address Family），
             *    常见的有：AF_INET：IPv4 地址家族，用于 Internet 地址。AF_INET6：IPv6 地址家族，用于 IPv6 地址。
             * 2）type 参数指定了套接字的类型，常见的有:SOCK_STREAM：流式套接字，用于 TCP 协议，提供可靠的、面向连接的通信。
             *                                    SOCK_DGRAM：数据报套接字，用于 UDP 协议，提供不可靠的、无连接的通信。
                                                  SOCK_RAW：原始套接字，用于直接访问网络协议，通常需要特权。
               3) protocol 参数指定了要使用的协议，通常可以设置为 0，让系统自动选择合适的协议。
             **/
            SOCKET sock = socket(AF_INET,SOCK_STREAM,0);

            if (IsSocketClosed(sock)) return INVALID_SOCKET;

            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);  /**htons() 函数将主机字节序转换为网络字节序。**/
            addr.sin_addr.s_addr = inet_addr(ip);  /**将 IP 地址转换为网络字节序并设置到 addr 结构体中。**/

            /**ioctlsocket() 是一个用于控制套接字（socket）行为的函数，通常用于套接字编程中。
             * 它允许程序员通过一系列的命令来控制套接字的各种属性和行为。
             * int ioctlsocket(SOCKET s, long cmd, u_long* argp);
             * s：套接字描述符，要对哪个套接字执行操作。
               cmd：一个控制命令，指定要执行的操作。
               argp：一个指向参数的指针，具体参数的类型和意义取决于 cmd。

               在这段代码中，首先将 mode 设置为 1，表示要将套接字设置为非阻塞模式。
               然后，通过调用 ioctlsocket() 函数，将 FIONBIO 控制命令应用于套接字 sock，以将其设置为非阻塞模式。**/
            ioctlsocket(sock,FIONBIO,&mode); mode = 0;


            /**connect 函数用于在客户端套接字上发起连接到服务器端套接字的请求，建立网络连接。这个函数通常用于创建 TCP 或 UDP 客户端程序。
             * int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
                sockfd：要连接的套接字的文件描述符。
                addr：指向目标服务器端地址信息的指针，通常是 struct sockaddr_in 或 struct sockaddr 类型的指针。sockaddr_in通常用于ipv4
                addrlen：addr 指向的地址结构体的大小，通常使用 sizeof(struct sockaddr_in) 来获取。
                返回值：如果连接成功，connect 返回 0。
                       如果连接失败，它会返回 -1，并设置全局变量 errno 来指示失败的原因。
                连接过程：
                        connect 函数会尝试建立与目标服务器的连接。
                        如果连接成功，套接字现在可以用于发送和接收数据。
                        如果连接失败，可以使用 errno 来确定失败的原因。
                阻塞：
                        默认情况下，connect 函数是阻塞的，它会一直等待连接成功或失败。
                        如果需要使用非阻塞连接，可以在调用 connect 前将套接字设置为非阻塞模式。
                错误处理：
                        如果 connect 返回 -1，你可以使用 errno 来查找失败的原因。
                        常见的错误包括 ECONNREFUSED（连接被拒绝）、ETIMEDOUT（连接超时）等。**/
            if (::connect(sock,(struct sockaddr *)(&addr),sizeof(addr)) == 0){
                ioctlsocket(sock,FIONBIO,&mode);
                return sock;
            }
#ifdef LINUX

            /**如果connect连接失败，执行下列操作
             * 针对 Linux，该代码创建了一个 epoll 实例，
             * 并将套接字添加到 epoll 集合中，关注 EPOLLOUT、EPOLLERR 和 EPOLLHUP 事件。
             * 使用 epoll_wait() 函数等待套接字就绪，如果在超时时间内有事件发生，且其中包含
             * EPOLLOUT 事件，那么说明连接成功，获取套接字错误状态并将套接字重新设置为阻塞模式，然后返回该套接字。
             */
            struct epoll_event ev;
            struct epoll_event evs;
            /**创建了一个 epoll 实例,原型如下
             * int epoll_create(int size);
            用于指定 epoll 实例的大小，但在现代的 Linux 内核中，这个参数已经不再使用，通常可以设置为任意正数。
             **/
            int handle = epoll_create(1);
            /**创建失败，它会关闭套接字并返回 INVALID_SOCKET**/
            if (handle < 0){
                SocketClose(sock);

                return INVALID_SOCKET;
            }
            /** 用于将 ev 结构体的内存清零，以确保没有未初始化的数据。**/
            memset(&ev,0, sizeof(ev));
            /**events字段用于表示文件描述符上发生的事件。
             * EPOLLIN：文件描述符可读（有数据可读取）。
                EPOLLOUT：文件描述符可写（可以发送数据）。           EPOLLOUT 的二进制表示是 00000001，只有最低位是1。
                EPOLLERR：文件描述符上有错误发生。                   EPOLLERR 的二进制表示是 00000010，只有次低位是1。
                EPOLLHUP：文件描述符的挂起事件（通常表示连接已关闭）。  EPOLLHUP 的二进制表示是 00000100，只有第三位是1
                得到二进制表示 00000111，这意味着同时设置了这三个标志位。
                然后，在检查事件时，你可以使用按位与操作来测试特定的标志位是否设置，以确定哪些事件已经发生。**/
            ev.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
            /** epoll_ctl 是 Linux 下用于控制 epoll 实例的函数之一，它主要用于向 epoll 实例中添加、修改或删除文件描述符以及关联的事件。
             * int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
               1）epfd：表示要进行操作的 epoll 实例的文件描述符。
               2）op：表示操作类型，可以是以下几种值之一：
                    EPOLL_CTL_ADD：将文件描述符 fd 添加到 epoll 实例中，以监视 event 中指定的事件。
                    EPOLL_CTL_MOD：修改已经添加到 epoll 实例的文件描述符 fd 的事件监视设置。
                    EPOLL_CTL_DEL：从 epoll 实例中删除文件描述符 fd，停止监视该文件描述符上的事件。
               3）fd：表示要添加、修改或删除的文件描述符。
               4）event：一个指向 struct epoll_event 结构的指针，用于指定要监视的事件类型以及与事件相关的数据。

             * 告诉 epoll 实例 handle 开始监视套接字 sock 上的特定事件，这些事件在 ev 结构中指定。
             * 一旦套接字上发生了这些事件中的任何一个，你将能够通过 epoll_wait 等函数获得通知，并采取适当的操作。
             * ev 结构初始化为监视以下事件：
                EPOLLOUT：当socket准备好写入数据时触发此事件，通常用于非阻塞套接字以避免写入阻塞。
                EPOLLERR：当socket上发生错误时触发此事件，例如连接重置。
                EPOLLHUP：当socket的挂起或关闭事件发生时触发此事件。
                一旦调用 epoll_ctl 成功，并将套接字添加到 epoll 实例中，你可以使用 epoll_wait 函数来等待发生这些事件，然后采取适当的处理步骤。**/
            epoll_ctl(handle,EPOLL_CTL_ADD,sock,&ev);

            /**epoll_wait 是 Linux 下用于等待事件发生的函数，
             * int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
                epfd：表示要等待事件的 epoll 实例的文件描述符。
                events：一个指向 struct epoll_event 数组的指针，用于存储发生了的事件的类型 events字段 以及相关的事件信息  data字段。
                maxevents：表示 events 数组的大小，即最多可以存储多少个事件。
                timeout：指定等待事件的超时时间，单位是毫秒。如果设置为 -1，则表示无限等待，直到有事件发生。如果设置为 0，则表示立即返回，不等待事件。

             * 用于等待事件的发生，最多等待 timeout 毫秒。如果在超时时间内发生了事件，它会将事件信息存储在 evs 中。
             * 检查返回值来判断是否发生了事件，如果返回大于 0，表示至少有一个事件发生；如果返回等于 0，表示超时；如果返回小于 0，表示出现了错误。
             * 阻塞模式：如果你将 epoll_wait 的 timeout 参数设置为一个非负数，例如 timeout 大于 0，那么 epoll_wait 将会以阻塞模式工作。
             * 它会等待指定的时间（以毫秒为单位）直到发生事件或者超时才返回。在这种模式下，如果没有事件发生，epoll_wait 会一直阻塞当前线程，直到有事件发生或者超时。
               非阻塞模式：如果你将 epoll_wait 的 timeout 参数设置为 0（或者更准确地说，设置为负数），那么 epoll_wait 将会以非阻塞模式工作。
               在这种模式下，它会立即返回，不会等待事件发生，而是立即告诉你当前没有事件发生。**/
            if (epoll_wait(handle,&evs,1,timeout) > 0){
                /**检查 evs.events 中是否包含 EPOLLOUT 事件,按位与，若包含，说明可写入，即连接成功**/
                if (evs.events & EPOLLOUT){
                    int res = FAIL;

                    socklen_t len = sizeof(res);
                    /**用于获取套接字选项值的函数，它用于查询套接字的特定属性或状态信息.
                     * sockfd：要查询选项的套接字文件描述符。
                    level：选项的级别，通常是 SOL_SOCKET（表示套接字级别选项）或其他协议级别。
                    optname：要查询的选项的名称，例如 SO_ERROR。
                    optval：用于存储选项值的缓冲区，通常是一个指向合适数据类型的指针。
                    optlen：输入输出参数，表示 optval 缓冲区的大小，同时也表示成功获取的选项值的大小。

                     是获取套接字的错误状态。例如，通过指定 optname 为 SO_ERROR，可以获取套接字的错误状态，通常用于检查套接字连接操作的结果。**/
                    getsockopt(sock,SOL_SOCKET,SO_ERROR,(char *)(&res),&len);
                    /**将套接字重新设置为阻塞模式。**/
                    ioctlsocket(sock,FIONBIO,&mode);
                    /**如果为0，表示连接成功，然后关闭 epoll 实例并返回套接字。**/
                    if (res == 0){
                        ::close(handle);

                        return sock;
                    }
                }
            }

            ::close(handle);
#else
            struct timeval tv;

			fd_set ws;
			FD_ZERO(&ws);
			FD_SET(sock, &ws);

			tv.tv_sec = timeout / 1000;
			tv.tv_usec = timeout % 1000 * 1000;

			if (select(sock + 1, NULL, &ws, NULL, &tv) > 0)
			{
				int res = ERROR;
				int len = sizeof(res);

				getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)(&res), &len);
				ioctlsocket(sock, FIONBIO, &mode);

				if (res == 0) return sock;
			}

#endif
            /***如果运行到了这里说明没有socket没有连接成功，关闭sock，返回INVALID_SOCKET**/
            SocketClose(sock);
            return INVALID_SOCKET;
        }


    public:
        /**关闭socket**/
        void close(){
            SocketClose(sock);
            sock = INVALID_SOCKET;
        }
        /**const 的作用是确保函数不会修改类的内部状态，即不会修改 sock 这个成员变量的值。**/
        bool isClosed() const{
            return IsSocketClosed(sock);
        }
        /**设置发送超时时间**/
        bool setSendTimeout(int timeout){
            return SocketSetSendTimeout(sock,timeout);
        }
        /**设置接收超时**/
        bool setRecvTimeout(int timeout){
            return SocketSetRecvTimeout(sock,timeout);
        }

        bool connect(const std::string & ip,int port,int timeout){
            close();
            sock = SocketConnectTimeout(ip.c_str(),port,timeout);
            return IsSocketClosed(sock) ? false : true;
        }

    public:
        /**将data中的数据写入socket**/
        int write(const void * data,int count){
            const char * str = (const char *)(data);
            int num = 0;    /**记录每次发送的字节数**/
            int times = 0;  /**发送发送成功但一次发送字节数小于等于8的次数**/
            int writed = 0; /**已成功发送的字节数**/

            while (writed < count){
                /**send() 函数是用于在socket上发送数据的函数。
                 * ssize_t send(int sockfd, const void *buf, size_t len, int flags);
                    sockfd：套接字文件描述符，指定要发送数据的套接字。
                    buf：要发送的数据的缓冲区地址。
                    len：要发送的数据的长度（以字节为单位）。
                    flags：用于指定发送操作的标志，通常可以设置为0。

                    send() 函数的返回值是已发送的字节数，如果出现错误则返回-1。
                    send() 函数通常用于阻塞式套接字（默认情况下）。如果套接字是非阻塞的，它可以在没有足够数据发送时立即返回，
                    并且返回值可能小于请求发送的数据长度。在非阻塞套接字上使用 send() 时需要谨慎处理返回值，因为需要检查返回值以确定实际发送了多少数据。
                **/
                /**将data中的数据写入到socket，每次从str+writed开始写**/
                if ((num = send(sock,str+writed,count-writed,0)) > 0){
                    if (num > 8){
                        times = 0;
                    } else{
                        if (++times > 100) return TIMEOUT;
                    }

                    writed += num;
                } else{
                    if (IsSocketTimeout()){
                        if (++times > 100) return TIMEOUT;
                        continue;
                    }
                    return NETERR;
                }
            }

            return writed;
        }
        /**从socket中读取数据，基本同上，有两种读取模式，completed和非completed**/
        int read(void * data,int count,bool completed){
            char * str = (char *)(data);
            if (completed){
                int num = 0;
                int times = 0;
                int readed = 0;

                while (readed < count){
                    /**recv() 是一个用于从套接字（Socket）接收数据的系统调用（函数）。它通常用于网络编程中，用于接收数据报文或消息。
                     * int recv(int sockfd, void *buf, size_t len, int flags);
                        sockfd：表示要接收数据的套接字文件描述符。
                        buf：指向存储接收数据的缓冲区的指针。
                        len：指定要接收的数据的最大长度。
                        flags：指定接收数据的附加选项，通常可以设置为 0。
                        返回值 如果 recv() 返回值大于 0，则表示成功接收了指定数量的字节数据。
                              如果 recv() 返回值等于 0，表示对端（通常是远程服务器）已经关闭了连接。
                              如果 recv() 返回值为 -1，表示发生了错误，此时可以使用 errno 全局变量获取错误码，以进一步确定错误的原因。

                     **/
                     /**将socket中的数据写入str+readed**/
                    if ((num = recv(sock,str+ readed,count-readed,0)) > 0){
                        if (num > 8){
                            times = 0;
                        } else{
                            if (++times > 100){
                                return TIMEOUT;
                            }
                        }
                        readed += num;
                    } else if (num==0){
                        return NETCLOSE;
                    } else{
                        if (IsSocketTimeout()){
                            if (++times > 100) return TIMEOUT;
                            continue;
                        }
                        return NETERR;
                    }
                }
                return readed;
            } else{
                int val = recv(sock,str,count,0);
                if (val > 0) return val;
                if (val == 0) return NETCLOSE;
                if (IsSocketTimeout()) return 0;

                return NETERR;
            }
        }
    };

    class Command {
        friend RedisConnect;

    protected:
        int status;
        std::string msg;
        vector<string> res;
        vector<string> vec;

    protected:
        int parse(const char * msg,int len){
            /**以 $ 开头，表示它是一个批定长度的字符串。它会调用 parseNode 函数来解析字符串，
             * 并根据解析的结果返回相应的状态码（如 OK、TIMEOUT、NOTFUND）。
             *
             * get命令会返回  "$9\r\nlzh111111\r\n"**/
            if (*msg == '$'){
                const char * end = parseNode(msg,len);

                if (end == NULL) return DATAERR;

                switch (end - msg) {
                    case 0 : return TIMEOUT;
                    case -1: return NOTFUND;
                }

                return OK;
            }
            /**跳过resp协议规定的首字符如$ + - :*/
            const char * str = msg + 1;
            /**返回"\r\n"首次出现的位置**/
            const char * end = strstr(str,"\r\n");
            /**如果无法找到结束标记，返回原始的 msg，表示解析失败。**/
            if (end == NULL) return TIMEOUT;
            /**如果消息以 +、- 或 : 开头，表示它是一个状态回复，通常用于表示成功、失败或状态码。
             * 根据消息的前缀字符，它设置相应的状态码和消息内容。**/
            if (*msg == '+' || *msg == '-' || *msg == ':'){
                this->status = OK;
                /**截取除了首字符以及结尾的"\r\n"之间的字符串***/
                this->msg = string(str,end);

                if (*msg == '+') return OK;
                if (*msg == '-') return FAIL;

                this->status = atoi(str);

                return OK;
            }


            /**消息以 "*" 开头，表示这是一个多行回复，通常用于表示多个元素的数组。它解析数组的元素数量，然后循环处理每个元素。
             * 每个元素都以 "$" 开头，后面跟着元素的长度。parseNode 被用来解析每个元素。解析完成后，它返回数组的长度。
             * 一般数组元素中都是字符串，所以用parseNode解析数组内元素**/
            if (*msg == '*'){
                /**查看数组中有几个元素**/
                int cnt = atoi(str);
                /**标记 RESP 响应字符串的末尾，以确保不超出字符串的长度。**/
                const char * tail = msg +len;
                /**因为是数组，所以返回有多个，用vec存储**/
                vec.clear();
                /**移动到数组中第一个元素的位置**/
                str = end + 2;
                /**遍历解析数组中元素**/
                while (cnt > 0){
                    /**如果这个元素还是数组，递归解析**/
                    if (*str == '*') return parse(str,tail-str);

                    end = parseNode(str,tail- str);
                    if (end == NULL) return DATAERR;
                    if (end == str)  return TIMEOUT;

                    str = end;
                    cnt--;
                }
                return res.size();
            }
            return DATAERR;
        }
        /**用于解析 RESP 响应中的字符串节点。**/
        const char * parseNode(const char * msg , int len){
            /**跳过resp协议规定的首字符如$ + - :*/
            const char * str = msg + 1;
            /**用于在一个字符串中查找另一个字符串，并返回第一次出现匹配的子字符串的指针。**/
            const char * end = strstr(str,"\r\n");
            /**如果无法找到结束标记，返回原始的 msg，表示解析失败。**/
            if (end == NULL) return msg;
            /**字符串的长度**/
            int sz = atoi(str);
            /**sz 为负数时，表示它是一个引用，msg + sz 实际上是指向之前出现的元素，而不是当前元素。这样可以有效地减少数据传输时的冗余。**/
            if (sz < 0) return msg + sz;
            /*第一个元素起始位置***/
            str = end + 2;
            /**结束位置**/
            end = str + sz + 2;
            /**如果 RESP 响应的长度超出了给定的数据长度 len，返回原始的 msg，表示解析失败。**/
            if (msg + len < end) return msg;
            /**将成功解析的 RESP 字符串内容存入 res**/
            res.emplace_back(string(str,str + sz));

            return end;
        }
    public:
        Command(){
            this->status = 0;
        }
        Command(const string & cmd){
            vec.emplace_back(cmd);
            this->status = 0;
        }
        void add(const char * val){
            vec.emplace_back(val);
        }
        void add(const string & val){
            vec.emplace_back(val);
        }

        /**调用上面那个**/
        template<class DATA_TYPE>
        void add(DATA_TYPE val){
            add(to_string(val));
        }

        /**递归调用 每次传入的参数中去掉了第一个参数 val**/
        template<class DATA_TYPE,class ...ARGS>
        void add(DATA_TYPE val,ARGS ...args){
            add(val);
            add(args...);
        }

    public:
        /**toString 函数，用于将一组字符串转换为符合 Redis 协议的字符串表示形式。**/
        string toString() const{
            /**ostringstream 对象 out，它用于构建最终的字符串表示形式。**/
            ostringstream out;
            /**添加一个表示命令参数数量的头部，例如，如果 vec 中有3个参数，头部将是 *3\r\n。**/
            out << "*" << vec.size() << "\r\n";
            /**对于 vec 中的每个字符串参数，执行以下步骤：
                添加一个表示字符串长度的标识符，例如，如果字符串的长度是10，标识符将是 $10\r\n。
                添加字符串的实际内容，然后添加 \r\n 表示字符串的结束。
             **/
            for (const string & item : vec) {
                out << "$" << item.length() << "\r\n" << item << "\r\n";
            }
            return out.str();
        }

        string get(int idx) const{
            return res.at(idx);
        }

        const vector<string> & getDataList() const{
            return res;
        }

        int getResult(RedisConnect * redis,int timeout){
            /**lambda函数，执行redis命令**/
            auto doWork = [&](){
                /**toString获取redis命令
                 * get命令    "*2\r\n$3\r\nget\r\n$5\r\nname2\r\n"
                 * set命令   ""*3\r\n$3\r\nset\r\n$4\r\nname\r\n$9\r\nlzh111111\r\n""
                 * 删除锁的命令 "*5\r\n$4\r\neval\r\n$93\r\nif redis.call('get',KEYS[1])==ARGV[1] then return redis.call('del',KEYS[1]) else return 0 end\r\n$1\r\n1\r\n$6\r\nlockey\r\n$26\r\n172.20.123.254:51235:51235\r\n"**/
                string msg = toString();
                /**获取socket连接**/
                Socket & sock = redis->sock;
                /**将命令字符串 msg 写入到与 Redis 服务器的socket连接中，如果写入失败（返回值小于 0），则返回 NETERR，表示网络错误。**/
                if (sock.write(msg.c_str(),msg.length()) < 0) return NETERR;

                int len = 0;           /**用于存储读取的数据长度**/
                int delay = 0;         /**用于记录超时时间**/
                int readed = 0;        /**用于记录已读取的数据长度**/
                char * dest = redis->buffer;  /**用于存储接收的数据**/
                const int maxsz = redis->memsz;
                /**确保没有读取超出指定最大长度的数据**/
                while (readed < maxsz){
                    /**从连接中读取数据，并将读取的数据存储到 dest 缓冲区中，
                     * dest中本身是可能有数据的，read后会覆盖dest到dest+len的数据，但是dest+len+1处可能还有字符，下面的dest[readed += len] = 0;就是在len+1处添加\0符号，终止**/
                    if ((len = sock.read(dest+readed,maxsz- readed, false)) < 0) return len;
                    /**表示暂时没有数据可读,增加 delay，若delay > timeout。说明超时，返回 TIMEOUT 表示响应超时**/
                    if (len == 0){
                        delay +=  SOCKET_TIMEOUT;
                        if (delay > timeout) return TIMEOUT;
                    } else{
                        /**终止符***/
                        dest[readed += len] = 0;
                        /**将读取到的数据传递给 parse 函数来解析响应数据,将数据存入res中。如果 parse 函数返回 TIMEOUT，表示需要继续等待更多数据。**/
                        if ((len = parse(dest,readed)) == TIMEOUT){
                            delay = 0;
                        } else{
                            return len;
                        }
                    }
                }
                return PARAMERR;
        };
            /**重置cmd的status和msg**/
            status = 0;
            msg.clear();
            redis->code = doWork();
            /**redis->code 小于 0说明出问题了  若执行成功，cmd.msg不会为空，会在dowork中的parse里设置**/
            if (redis->code < 0 && msg.empty()){
                switch (redis->code) {
                    case SYSERR:
                        msg = "system error";
                        break;
                    case NETERR:
                        msg = "network error";
                        break;
                    case DATAERR:
                        msg = "protocol error";
                        break;
                    case TIMEOUT:
                        msg = "response timeout";
                        break;
                    case NOTFUND:
                        msg = "element not found";
                        break;
                    default:
                        msg = "unknown error";
                }
            }

            redis->status = status;
            redis->msg = msg;


            return redis->code;
    };




};

protected:
    /**code 通常用来表示具体的执行结果或错误码。在 RedisConnect 类中，code 变量用于存储执行命令或操作的返回状态码，
     * 例如成功执行时可能是 0，不同的错误情况可能对应不同的非零状态码。这个状态码可以帮助开发人员判断具体执行过程中是否出现了问题。**/
    int code = 0;   /**错误代码**/
    int port = 0;   /**redis服务器端口号**/
    int memsz = 0;  /**缓存数据的内存大小**/
    /**status 变量通常用于表示一个更宽泛的状态或标志，它可能不是具体的数字错误码。在 RedisConnect 类中，status
     * 变量可能表示连接状态或操作状态的标志，比如是否成功连接到 Redis 服务器、是否成功获取分布式锁等。这个状态通常是布尔值或枚举类型，
     * 更容易理解操作的成功与否。**/
    int status = 0; /**连接状态**/
    int timeout = 0;/**超时时间**/
    char * buffer = NULL; /**数据缓冲区**/

    string msg;      /**错误信息**/
    string host;     /**服务器主机**/
    Socket sock;     /**socket**/
    string passwd;   /**密码**/

public:
    ~RedisConnect(){
        close();
    }

public:
    /**获取连接状态**/
    int getStatus() const{
        return status;
    }
//    获取错误代码。
    int getErrorCode() const{
        if (sock.isClosed()) return FAIL;
        return code < 0 ? code : 0;
    }
//    获取错误消息。
    string getErrorString() const{
        return msg;
    }

public:
    /**关闭连接并释放资源。
    该方法用于关闭与 Redis 服务器的socket，同时释放内存资源。如果之前分配了缓冲区，也会释放缓冲区。**/
    void close(){
        if (buffer){
            delete[] buffer;
            buffer = NULL;
        }
        sock.close();
    }
    /**重新连接到 Redis 服务器。
     * 如果成功重新连接并进行身份验证（如果设置了密码），则返回true；否则，返回false。**/
    bool reconnect(){
        if (host.empty()) return false;

        return connect(host,port,timeout,memsz) && auth(passwd) > 0;
    }
    /**执行 Redis 命令并返回执行结果。**/
    int execute(Command & cmd){
        return cmd.getResult(this,timeout);
    }
    /**同上
     * val：表示要执行的 Redis 命令的参数。
        args...：可选的额外参数。

        ...args 表示将参数包 args 展开成单独的参数。
        args... 表示将多个参数打包成一个参数包。**/
    template<class DATA_TYPE,class ...ARGS>
    int execute(DATA_TYPE val,ARGS ...args){
        /**初始化一个Command对象**/
        Command cmd;
        /**将参数不断地放入cmd对象中的vec中["set","name","lzh111111"]**/
        cmd.add(val,args...);

        return cmd.getResult(this,timeout);
    }
    /**同上上
     * vec：一个字符串向量，用于存储命令的执行结果。
        val：表示要执行的 Redis 命令的参数。
        args...：可选的额外参数。**/
    template<class DATA_TYPE, class ...ARGS>
    int execute(vector<string>& vec, DATA_TYPE val, ARGS ...args)
    {
        Command cmd;

        cmd.add(val, args...);

        cmd.getResult(this, timeout);

        if (code > 0) std::swap(vec, cmd.res);

        return code;
    }
    /**用于连接到指定的主机和端口，并进行一些初始化操作。**/
    bool connect(const string & host,int port,int timeout = 3000,int memsz = 2 * 1024 * 1024){
        /**首先调用 close() 函数来关闭可能已经存在的连接。**/
        close();
        /**如果连接成功，进行后续操作**/
        if (sock.connect(host,port,timeout)){
            /**设置发送/接收超时时间**/
            sock.setSendTimeout(SOCKET_TIMEOUT);
            sock.setRecvTimeout(SOCKET_TIMEOUT);
            /**设置host，port,缓冲区大小，超时时间，缓冲区**/
            this->host = host;
            this->port = port;
            this->memsz = memsz;
            this->timeout = timeout;
            this->buffer = new char [memsz + 1];
        }
        /**若缓冲区申请成功，说明连接成功**/
        return buffer ? true: false;
    }

public:
    int ping(){
        return execute("ping");
    }

    int del(const string & key){
        return execute("del",key);
    }

    int ttl(const string & key){
        return execute("ttl",key) == OK ? status : code;
    }

    int hlen(const string & key){
        return execute("hlen",key) == OK ? status : code;
    }
    /**密码登录**/
    int auth(const string & passwd){
        this->passwd = passwd;
        /**空密码就说明没有密码，登录成功**/
        if (passwd.empty()) return OK;
        return execute("auth",passwd);
    }

    int get(const string & key,string & val){
        vector<string> vec;

        if (execute(vec,"get",key) < 0) return code;
        val = vec[0];
        return code;
    }

    int decr(const string& key, int val = 1)
    {
        return execute("decrby", key, val);
    }

    int incr(const string& key, int val = 1)
    {
        return execute("incrby", key, val);
    }

    int expire(const string& key, int timeout)
    {
        return execute("expire", key, timeout);
    }
    /**返回所有 key，返回的key存放在vec中**/
    int keys(vector<string>& vec, const string& key)
    {
        return execute(vec, "keys", key);
    }
    /**哈希数据删除field**/
    int hdel(const string& key, const string& field)
    {
        return execute("hdel", key, field);
    }
    /**哈希数据获取field对应的value**/
    int hget(const string& key, const string& field, string& val)
    {
        vector<string> vec;

        if (execute(vec, "hget", key, field) <= 0) return code;

        val = vec[0];

        return code;
    }

    /**设置key，根据timout执行 setex还是set**/
    int set(const string & key,const string & val,int timeout = 0){
        return timeout > 0 ? execute("setex",key,timeout,val) : execute("set",key,val);
    }
    /**哈希数据设置field与对应的value**/
    int hset(const string & key,const string & field,const string & val){
        return execute("hset",key,field,val);
    }

public:
    /**列表数据删除，弹出数据，调用lpop，也就是左边弹出**/
    int pop(const string & key,string & val){
        return lpop(key,val);
    }
    /**列表左弹出**/
    int lpop(const string & key,string & val){
        vector<string> vec;
        if (execute(vec,"lpop",key) <= 0) return code;
        val = vec[0];
        return code;
    }
    /**列表右弹出**/
    int rpop(const string& key, string& val)
    {
        vector<string> vec;

        if (execute(vec, "rpop", key) <= 0) return code;

        val = vec[0];

        return code;
    }
    /**列表数据增加，添加数据**/
    int pust(const string & key,const string & val){
        return rpush(key,val);
    }
    /**左添加**/
    int lpush(const string & key,const string & val){
        return execute("lpush",key,val);
    }
    /**右添加**/
    int rpush(const string & key,const string & val){
        return execute("rpush",key,val);
    }
    /**列表数据查看，start与end之间的数据，左闭右闭**/
    int range(vector<string> & vec,const string & key,int start,int end){
        return execute(vec,"lrange",key,start,end);
    }
    /**lrange**/
    int lrange(vector<string> & vec,const string & key,int start,int end){
        return execute("lrange",key,start,end);
    }

public:
    /**有序数组删除**/
    int zrem(const string & key,const string & field){
        return execute("zrem",key,field);
    }

    int zadd(const string & key,const string & field,int score){
        return execute("zadd",key,score,field);
    }

    int zrange(vector<string> & vec,const string & key,int start,int end,bool withsore = false){
        return withsore ? execute(vec,"zrange",key,start, end,"withscores") : execute(vec,"zrange",key,start,end);
    }
public:
    /**这个重载用于执行 Lua 脚本，而不涉及键（KEYS）和参数（ARGV）数组。这是一个最基本的执行 Lua 脚本的方式。**/
    template<class ...ARGS>
    int eval(const string & lua){
        vector<string> vec;
        return eval(lua,vec);
    }

    /**调用下面的eval
     * 这个重载允许执行 Lua 脚本，并传递一个键（KEYS[1]）和参数（ARGV[1]）给 Lua 脚本。在这种情况下，key 是唯一的键，args 可以是零个或多个附加参数。**/
    template<class ...ARGS>
    int eval(const string & lua,const string & key,ARGS ...args){
        vector<string> vec;
        vec.emplace_back(key);
        return eval(lua,vec,args...);
    }
    /**调用下面的eval
     * 这个重载允许执行 Lua 脚本，并传递一个键数组（KEYS）以及参数（ARGV）给 Lua 脚本。这样可以传递多个键，也可以传递零个或多个附加参数。**/
    template<class ...ARGS>
    int eval(const string & lua,const vector<string> & keys,ARGS ...args){
        vector<string> vec;
        return eval(vec,lua,keys,args...);
    }
    /**这个重载是最通用的，它允许传递自定义的键数组和参数到 Lua 脚本，并将执行结果存储在 vec 中。**/
    template<class ...ARGS>
    int eval(vector<string> & vec,const string & lua,const vector<string> & keys,ARGS ...args){
        int len = 0;
        Command cmd("eval");
        cmd.add(lua);
        cmd.add(len = keys.size());
//        int len1 = cmd.vec.size();

        if (len-- > 0){
            for (int i = 0; i < len; ++i) {
                cmd.add(keys[i]);
            }
            /**之所以这么写，是因为add()没有单独添加额外参数的重载形式，
             * 调用void add(DATA_TYPE val,ARGS ...args){**/
            cmd.add(keys.back(),args...);
        }
//        len1 = cmd.vec.size();

        cmd.getResult(this,timeout);
        if (code > 0) swap(vec,cmd.res);

        return code;
    }

    string get(const string & key){
        string res;
        get(key,res);
        return res;
    }

    string hget(const string & key,const string & field){
        string res;
        hget(key,field,res);
        return res;
    }

    const char * getLockId() {
        /**定义了一个线程局部的字符数组id，用于存储锁的唯一标识符。
         * 使用 thread_local 保证了锁的唯一标识符对于每个线程是独立的，避免了线程之间的竞争和冲突。**/
        thread_local char id[0xFF] = {};
        auto GetHost = []() {
            char hostname[0xFF];
            /**获取当前主机的名称，存储在hostname中**/
            if (gethostname(hostname, sizeof(hostname)) < 0) return "unkonw host";
            /**根据主机名获取主机的IP地址信息，存储在data中。通常用于进行网络编程中的主机名解析。
             * gethostbyname() 函数返回一个 struct hostent 结构指针，其中包含了与指定主机名相关的信息，包括 IP 地址。这个结构通常包括以下字段：
                h_name：官方主机名。                               "Present"
                h_aliases：主机的别名列表。
                h_addrtype：地址类型（通常为 AF_INET，表示 IPv4）。   2
                h_length：地址的字节数。                            4
                h_addr_list：主机的 IP 地址列表。通常，IP 地址以二进制形式表示，可以通过 inet_ntoa() 函数将其转换为字符串。
             **/
            struct hostent *data = gethostbyname(hostname);
            /**将IP地址转换为字符串格式，返回主机的IP地址。**/
            return (const char *) inet_ntoa(*(struct in_addr *) (data->h_addr_list[0]));
        };

        if (*id == 0){
#ifdef LINUX
            /**将主机名（或IP地址）、当前进程ID和当前线程ID等信息格式化成一个字符串，并将其存储在id中。这个字符串将作为锁的唯一标识符。**/
            snprintf(id, sizeof(id)-1,"%s:%ld:%ld",GetHost(),(long)getpid(),(long) syscall(SYS_gettid));
#else
            snprintf(id, sizeof(id) - 1, "%s:%ld:%ld", GetHost(), (long)GetCurrentProcessId(), (long)GetCurrentThreadId());
#endif
        }
        return id;
    }
    /**
    这段代码是用来实现分布式锁的解锁操作。解锁的关键在于确保只有持有锁的客户端才能够释放锁，而其他客户端不能随意释放锁。

    通过 Lua 脚本执行 Redis 命令。

    redis.call 是 Redis Lua 脚本中的一个内建函数，用于调用 Redis 命令。在 Lua 脚本中，可以使用 redis.call 来执行 Redis 命令，
     就像在普通的 Redis 命令行中执行一样。例如redis.call('get', 'mykey')。这允许你在 Lua 脚本中利用 Redis 数据存储和处理的能力。

     KEYS 是 Lua 脚本中的一个特殊数组，用于表示传递给 Lua 脚本的 Redis 键。在 Redis Lua 脚本中，
     KEYS 数组允许你引用传递给脚本的 Redis 键（key）。KEYS 数组的索引从 1 开始。
     在这里KEYS[1] 的含义是使用传递给脚本的第一个 Redis 键，通常在解锁分布式锁的情况下，KEYS[1] 将包含用于加锁的键。通过使用 KEYS[1]，
     你可以在 Lua 脚本中引用和操作这个键，例如检查它的值是否与你期望的值相匹配，然后执行相应的操作，例如删除该键。

     ARGV 是 Lua 脚本中的另一个特殊数组，用于表示传递给 Lua 脚本的参数。与 KEYS 数组类似，ARGV 允许你引用传递给脚本的参数。
     同样，Lua 数组的索引从 1 开始。
     在你的 Lua 脚本中，ARGV[1] 表示使用传递给脚本的第一个参数，即数组的第一个元素。通常情况下，ARGV[1] 包含了用于解锁分布式锁的信息，
     通常是加锁时设置的标识，以确保只有持有相同标识的客户端才能成功解锁。

    通过这个 Lua 脚本，只有持有锁的客户端才能够成功解锁，其他客户端无法释放锁。这是一种非常安全且保证锁的一致性的解锁方式。
     **/
    bool unlock(const string & key){
        const char * lua  = "if redis.call('get',KEYS[1])==ARGV[1] then return redis.call('del',KEYS[1]) else return 0 end";
        return eval(lua,key,getLockId()) > 0 && status == OK;
    }

    /**一个键（用于标识锁）和一个超时时间（默认为30秒）作为参数**/
    bool lock(const string & key,int timeout=30){
        /**将传入的超时时间转换为毫秒**/
        int delay = timeout * 1000;
        for (int i = 0; i < delay; i+=10) {
            if (execute("set",key,getLockId(),"nx","ex",timeout) >= 0) return true;
//            sleep(10);
            Sleep(10);
        }
        return false;
    }


protected:
    /** [捕获列表] (参数列表) -> 返回值类型 {
            // 函数体
        }
     * Lambda 表达式的捕获列表用于指定在 Lambda 内部可以访问的外部变量。捕获列表的形式为 []，在括号内可以使用以下不同的方式来指定捕获变量：
    不捕获任何变量：[]，表示 Lambda 不会访问任何外部变量。
    捕获所有变量（按值捕获）：[=]，表示 Lambda 可以访问当前作用域内的所有变量，但是它们会被按值捕获，Lambda 内部对这些变量的修改不会影响外部。
    捕获所有变量（按引用捕获）：[&]，表示 Lambda 可以访问当前作用域内的所有变量，它们会被按引用捕获，Lambda 内部对这些变量的修改会影响外部。
    指定捕获的变量：[x, y]，表示只捕获 x 和 y 两个变量。
    按值捕获指定变量：[=, &x]，表示按值捕获所有变量，但 x 会按引用捕获。
    按引用捕获指定变量：[&, y]，表示按引用捕获所有变量，但 y 会按值捕获。
    指定捕获变量并设置它们的捕获方式：[x, &y]，表示只捕获 x 和 y 两个变量，其中 x 按值捕获，y 按引用捕获。**/
    virtual shared_ptr<RedisConnect> grasp() const{

        /**静态初始化一个连接池ResPool<RedisConnect>  由后面的lambda函数初始化。
         * 调用的构造函数是     ResPool(function<shared_ptr<T>()> func, int maxlen = 8, int timeout = 60)
                            {
                                this->timeout = timeout;
                                this->maxlen = maxlen;
                                this->func = func;  这个形参对应的实参是lambda函数
                            }
         * **/
        static ResPool<RedisConnect> pool  ([&](){
            /**创建了一个名为 redis 的智能指针，指向了一个新创建的 RedisConnect 对象，并使用 make_shared 函数进行初始化。
             * make_shared 是 C++ 中用于创建智能指针的函数，它会动态分配内存来存储对象，并返回一个指向该对象的智能指针。**/
           shared_ptr<RedisConnect> redis = make_shared<RedisConnect>();
            if (redis && redis->connect(host,port,timeout,memsz)){
                if (redis->auth(passwd)) return redis;
            }
            return redis = NULL;
        },POOL_MAXLEN);

        /**获取一个redis连接**/
        shared_ptr<RedisConnect> redis = pool.get();

        if (redis && redis->getErrorCode()){
            pool.disable(redis);
            /**失败就递归获取**/
            return grasp();
        }

        return redis;
    }

public:
    /**用于检查是否可以使用 Redis 连接库。它检查连接库的模板对象中的端口是否已配置。
     * 如果端口大于0，表示可以使用连接库，返回true；否则返回false。**/
    static bool CanUse(){
        return GetTemplate()->port > 0;
    }
    /**返回一个RedisConnect对象的指针。是静态类型的RedisConnect 实例，用于保存配置信息和管理连接。**/
    static RedisConnect * GetTemplate(){
        static RedisConnect redis;
        return &redis;
    }

    static shared_ptr<RedisConnect> Instance(){
        /**单例模式
         * 确保只有一个全局唯一的RedisConnect对象存在。这个RedisConnect提供连接所需要的配置
         * 线程池里的连接共享这个配置，不必单独为每个连接设置连接属性。
         *
         * GetTemplete()Instance() 返回的是一个指向 RedisConnect 对象的 shared_ptr 智能指针，
         * 这里GetTemplete()返回的是Setup中创建的那个单例RedisConnect ，这个RedisConnect指针调用grasp()**/
        return GetTemplate()->grasp();
    }
    /**用于设置 Redis 连接库的配置信息。
     * 并没有建立socket连接，建立连接是在Instance()实现的
     * @return 先调用GetTemplete()获取一个RedisConnect对象，
     * 它接受host主机名（或 IP 地址）、port端口号、密码、超时时间和内存大小作为参数，
     * 并将这些配置信息存储在连接库的模板对象中。在 Linux 下，它还忽略了SIGPIPE信号，以避免因管道破裂而导致程序崩溃。**/
    static void Setup(const string & host,int port,const string & passwd = "",int timeout = 3000,int memsz = 2 * 1024 * 1024){
#ifdef LINUX
        /**signal()处理信号函数，第一个参数是接受的信号，第二个参数是接收到对应信号要进行的操作，这里是忽略SIGPIPE信号
         *以下情况会触发SIGPIPE信号
         * 1）管道和FIFO（有名管道）操作：如果你在一个管道（包括匿名管道和有名管道）上写入数据，而没有进程在读取这些数据，这将导致 SIGPIPE 信号。

           2）在已经关闭的文件描述符上写：如果你试图向一个已经关闭的文件描述符（包括文件、套接字、管道等）写数据，操作系统会检测到这一情况并向进程发送 SIGPIPE 信号。

           3）使用内存映射的共享内存：在一些共享内存的情况下，如果你写入一个已经分离的共享内存区域，这也会触发 SIGPIPE 信号。
         *
         * 在这里本项目主要处理的是第二种情况，防止向已经关闭的socket连接输入数据时直接导致程序中断的情况，会忽略这个错误
         * **/
        signal(SIGPIPE,SIG_IGN);
#else
        WSADATA data; WSAStartup(MAKEWORD(2, 2), &data);
#endif
        RedisConnect * redis  = GetTemplate();
        redis->host = host;
        redis->port = port;
        redis->memsz = memsz;
        redis->passwd = passwd;
        redis->timeout = timeout;
    }
};



int RedisConnect::POOL_MAXLEN = 8;
int RedisConnect::SOCKET_TIMEOUT = 10;
#endif //REDISCONNECT_REDISCONNECT_MYSELF_H





