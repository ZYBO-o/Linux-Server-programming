## 多路I/O复用

多路IO转接服务器也叫做多任务IO服务器。该类服务器实现的主旨思想是， **不再由应用程序自己监视客户端连接，取而代之由内核替应用程序监视文件描述符。** 这对提高程序的性能至关重要。通常，网络程序在下列情况下需要使用I/O 复用技术。

+ 客户端程序需要同时处理多个socket。比如非阻塞connect技术。
+ 客户端程序要同时处理用户输入和网络连接。
+ TCP服务器要同时监听socket和连接socket。
+ 服务器需要同时处理TCP和UDP请求
+ 服务器需要同时监听多个端口，或者处理多种服务。

需要指出的的是，I/O复用虽然能同时监听多个文件描述符，但它本身是阻塞的。 而**且当多个文件描述符同时就绪时，如果不采用额外的措施，程序就只能按照顺序依次处理其中的每一个文件描述符，这使得服务器程序看起来像是串行工作的。**  :diamond_shape_with_a_dot_inside:  如果要实现并发，只能使用多进程或者多线程等编程手段。 

Linux下实现I/O复用的系统调用主要有： 

:small_blue_diamond: select

:small_blue_diamond: ​poll

:small_blue_diamond: ​epoll



## select系统调用

select系统调用的用途是：在一段指定时间内，监听用户感兴趣的文件描述符上的可读，可写，和异常等时间。

### 1.API解析

```c
/* According to POSIX.1-2001, POSIX.1-2008 */
#include <sys/select.h>

/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);

struct timeval {
  	long tv_sec; /* seconds */
  	long tv_usec; /* microseconds */
};

//下面的宏访问fd_set结构体中的位
void FD_ZERO(fd_set *set); 			//把文件描述符集合里所有位清0
void FD_CLR(int fd, fd_set *set); 	//把文件描述符集合里fd清0
int FD_ISSET(int fd, fd_set *set); 	//测试文件描述符集合里fd是否置1
void FD_SET(int fd, fd_set *set); 	//把文件描述符集合里fd位置1

#include <sys/select.h>

int pselect(int nfds, fd_set *readfds, fd_set *writefds,
            fd_set *exceptfds, const struct timespec *timeout,
            const sigset_t *sigmask);
```

:diamonds: **参数：**

+ nfds： 指定被监听的文件描述符的总数。它通常被设置为select 监听的所有文件描述符中的最大值加1，因为文件描述符是从0开始计数的。

+ readfds： 监控有读数据到达文件描述符集合，传入传出参数

+ writefds： 监控写数据到达文件描述符集合，传入传出参数

+ exceptfds： 监控异常发生达文件描述符集合,如带外数据到达异常，传入传出参数

+ timeout： 定时阻塞监控时间，3种情况

  1. NULL，永远等下去

  2. 设置timeval，等待固定时间

  3. 设置timeval里时间均为0，检查描述字后立即返回，轮询

:diamonds: ​**返回值：**
+ 成功： **所监听的所有 监听集合中，满足条件的总数**
+ 失败： 返回 -1 并设置errno



### 2.文件描述符就绪条件

:diamonds: **下面情况下 socket 可读**

+ socket接收缓冲区中的数据字节数大于等于套接字接收缓冲区低水位标记SO_RCVLOWAT的当前大小。 此时可以无阻塞地读该socket，对这样的套接字执行读操作不会阻塞并将返回一个大于0的值。
+ socket的读半部关闭（也就是接收了FIN的TCP连接）。对这样的套接字的读操作将不阻塞并返回0（也就是返回EOF）。
+ 监听socket上有了新的连接。对这样的套接字的accept通常不会阻塞。
+ Socket上有一个套接字错误待处理。对这样的套接字的读操作将不阻塞并返回-1（也就是返回一个错误），同时把errno设置成确切的错误条件。 这些错误可以通过getsockopt来读取和清除。

:diamonds: **下面情况下 socket 可写**

+ Socket发送缓冲区中的可用空间字节数大于等于套接字发送缓冲区低水位标记SO_SNDLOWAT的当前大小，并且或者该套接字已连接，或者该套接字不需要连接（如UDP套接字）。此时可以无阻塞地写该socket，并且写操作返回的字节数大于0。
+  socket的写半部关闭。对这样的套接字的写操作将产生SIGPIPE信号。
+ socket使用非阻塞式connect的套接字已建立连接，或者connect已经以失败告终。
+ socket上有一个套接字错误待处理。对这样的套接字的写操作将不阻塞并返回-1（也就是返回一个错误。



### 3.select的优缺点

:diamonds: **优点：**

1. select目前几乎在所有的平台上支持，其良好跨平台支持也是它的一个优点。

:diamonds: **缺点：**

1. select能监听的文件描述符个数 **受限于FD_SETSIZE** ,一般为1024，单纯改变进程打开的文件描述符个数并不能改变select监听文件个数。 `可以通过修改宏定义甚至重新编译内核的方式提升这一限制`，但是这样也会造成效率的降低。

2. 解决1024以下客户端时使用select是很合适的，但如果链接客户端过多，select采用的是轮询模型，会大大降低服务器响应效率，不应在select上投入更多精力

   > 当套接字比较多的时候，每次select()都要通过遍历FD_SETSIZE个socket来完成调度，不管哪个socket是活跃的，都遍历一遍。这会浪费很多CPU时间。
   >
   > `如果能给套接字注册某个回调函数，当他们活跃时，自动完成相关操作，那就避免了轮询`，这正是epoll与kqueue做的。

3. **文件描述符监听集合** 与 **返回后的满足监听条件的集合** 是一个集合，每次操作都需要将原有的集合进行保存。





## poll系统调用

> poll系统调用和select类似，也是在指定时间内轮询一定数量的文件描述符，以测试其中是否有就绪者。

### 1.API解析

```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <signal.h>
#include <poll.h>

int ppoll(struct pollfd *fds, nfds_t nfds,
          const struct timespec *tmo_p, const sigset_t *sigmask);
```

:diamonds: ​**参数：**

+ fds： pollfd结构类型的数组，它指定所有我们感兴趣的文件描述符上发生的可读，可写和异常等事件。

  ```c
  struct pollfd {
      int fd; /* 文件描述符 */
  		short events; /* 监控的事件 */
  		short revents; /* 监控事件中满足条件返回的事件，由内核填充 */
  };
  ```

  + poll支持的事件类型如下表所示：

    <div align = center><img src="../图片/UNIX52.png" width="800px" /></div>

    :small_blue_diamond:  Linux内核2.6.17开始，GNU为poll增加了 POLLRDHUP 事件。它在socket上接收到对方关闭连接的请求之后触发。

+ nfds： 指定被监听时间集合fds的大小。

+ timeout： 毫秒级等待
  + -1：阻塞等待，#define INFTIM -1         Linux中没有定义此宏
  + = 0：立即返回，不阻塞进程
  + \> 0：等待指定毫秒数，如当前系统时间精度不够毫秒，向上取值

:diamonds: ​**返回值：**

+ 成功： **所监听的所有 监听集合中，满足条件的总数**
+ 失败： 返回 -1 并设置errno

:diamonds: ​**简单的使用案例**

```c
struct pollfd fds[100];
//初始化
fds[0].fd = listenfd;
fds[0].events = POLLIN;
fds[1].fd = fd1;
fds[1].events = POLLIN;
fds[2].fd = fd2;
fds[2].events = POLLIN;
fds[3].fd = fd3;
fds[3].events = POLLIN;
fds[4].fd = fd4;
fds[4].events = POLLIN;
//调用poll函数
poll(fds, 5, -1);
```

:diamonds: ​**修改文件描述符的上限数目**

+ 可以使用cat命令查看一个进程可以打开的socket描述符上限。

  ```bash
  cat /proc/sys/fs/file-max
  ```

+ 如有需要，可以通过修改配置文件的方式修改该上限值。

  ```bash
  sudo vi /etc/security/limits.conf
  ```

+ 在文件尾部写入以下配置,soft软限制，hard硬限制。如下图所示。

  <div align = center><img src="../图片/UNIX41.png" width="350px" /></div>


### 2.poll的优缺点

**优点：**

+ 可以修改文件描述符的上限数目，优于select
+ 将监听集合 和 返回集合 实现了分离，不像select那么麻烦，搜索范围更小

**缺点：**

+ 但还是需要将数组全部变量，找出已连接的文件描述符，而不是直接返回满足的文件描述符数组。
