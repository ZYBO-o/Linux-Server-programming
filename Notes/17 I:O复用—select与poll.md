## 多路I/O转接服务器

多路IO转接服务器也叫做多任务IO服务器。该类服务器实现的主旨思想是， **不再由应用程序自己监视客户端连接，取而代之由内核替应用程序监视文件。**

主要使用的方法有三种：

+ select
+ poll
+ epoll



## select

### 1.函数解析

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

void FD_ZERO(fd_set *set); 			//把文件描述符集合里所有位清0
void FD_CLR(int fd, fd_set *set); 	//把文件描述符集合里fd清0
int FD_ISSET(int fd, fd_set *set); 	//测试文件描述符集合里fd是否置1
void FD_SET(int fd, fd_set *set); 	//把文件描述符集合里fd位置1

#include <sys/select.h>

int pselect(int nfds, fd_set *readfds, fd_set *writefds,
            fd_set *exceptfds, const struct timespec *timeout,
            const sigset_t *sigmask);
```

+ **参数：**

  + nfds： 监控的文件描述符集里最大文件描述符加1，因为此参数会告诉内核检测前多少个文件描述符的状态

  + readfds： 监控有读数据到达文件描述符集合，传入传出参数

  + writefds： 监控写数据到达文件描述符集合，传入传出参数

  + exceptfds： 监控异常发生达文件描述符集合,如带外数据到达异常，传入传出参数

  + timeout： 定时阻塞监控时间，3种情况

    1. NULL，永远等下去

    2. 设置timeval，等待固定时间

    3. 设置timeval里时间均为0，检查描述字后立即返回，轮询

+ **返回值：**
  + 成功： **所监听的所有 监听集合中，满足条件的总数**
  + 失败： 返回 -1 并设置errno

### 2.select注意事项

> 建立连接请求的文件描述符对应读事件。

1. select能监听的文件描述符个数 **受限于FD_SETSIZE** ,一般为1024，单纯改变进程打开的文件描述符个数并不能改变select监听文件个数

2. 解决1024以下客户端时使用select是很合适的，但如果链接客户端过多，select采用的是轮询模型，会大大降低服务器响应效率，不应在select上投入更多精力
3. **文件描述符监听集合** 与 **返回后的满足监听条件的集合** 是一个集合，每次操作都需要将原有的集合进行保存。





## poll

### 1.函数解析

```c
#include <poll.h>

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

struct pollfd {
    int fd; /* 文件描述符 */
		short events; /* 监控的事件 */
		short revents; /* 监控事件中满足条件返回的事件 */
};
//POLLIN				普通或带外优先数据可读,即POLLRDNORM | POLLRDBAND
//POLLRDNORM		数据可读
//POLLRDBAND		优先级带数据可读
//POLLPRI 			高优先级可读数据
//POLLOUT				普通或带外数据可写
//POLLWRNORM		数据可写
//POLLWRBAND		优先级带数据可写
//POLLERR 			发生错误
//POLLHUP 			发生挂起
//POLLNVAL 			描述字不是一个打开的文件


#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <signal.h>
#include <poll.h>

int ppoll(struct pollfd *fds, nfds_t nfds,
          const struct timespec *tmo_p, const sigset_t *sigmask);
```

+ **参数：**

  + fds： 结构体数组的首地址
  + nfds： 数组中元素的个数
  + timeout： 毫秒级等待
    + -1：阻塞等，#define INFTIM -1         Linux中没有定义此宏
    + = 0：立即返回，不阻塞进程
    + \> 0：等待指定毫秒数，如当前系统时间精度不够毫秒，向上取值

+ **返回值：**

  + 成功： **所监听的所有 监听集合中，满足条件的总数**
  + 失败： 返回 -1 并设置errno

+ **简单的使用案例**

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

  

+ **修改文件描述符的上限数目**

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
