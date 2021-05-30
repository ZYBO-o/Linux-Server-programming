

# 文件I/O

UNX系统中的大多数文件只需用到5个函数:open、read、 write、 lseek以及close。

本章描述的函数经常被称为不带缓冲的I/O( unbuffered I/O,与将在第5章中说明的标准I/O函数相对照)。术语不带缓冲指的是每个read和 write都调用内核中的一个系统调用。

只要涉及在多个进程间共享资源,原子操作的概念就变得非常重要。我们将通过文件IO和open函数的参数来讨论此概念。

## 一.文件描述符

对于内核而言,**所有打开的文件都通过文件描述符引用**。文件描述符是一个非负整数。当打开一个现有文件或创建一个新文件时,内核向进程返回一个文件描述符。

按照惯例,UNIX系统shell把**文件描述符0与进程的标准输入关联,文件描述符1与标准输出关联,文件描述符2与标准错误关联**。这是各种shel以及很多应用程序使用的惯例.

在符合POSIX.1的应用程序中，幻数0，1，2虽然已被标准化，但还是应该换成常量STDIN_FILENO，STDOUT_FILENO和STDERR_FILENO来提高可读性。这些常量定义在有文件<unistd.h>中。

## 二.函数open和openat

调用open或openat函数用于打开或者创建一个文件。

```c
#include <fcntl.h> 
int open(const char *path, int oﬂag, ... /* mode_t mode */ ); 
int openat(int fd, const char *path, int oﬂag, ... /* mode_t mode */ ); 
//Both return: ﬁle descriptor if OK, −1 on error
```

我们将最后一个参数写为…,ISO C用这种方法表明余下的参数的数量及其类型是可变的。对于open函数而言,仅当创建新文件时才使用最后这个参数。

+ path参数是要打开或创建文件的名字。
+ oflag参数可用来说明此函数的多个选项。用一个或者多个常量“或”运算构成oflag参数
  + oflags参数表示打开文件所采用的操作，我们需要注意的是：必须指定以下五个常量的一种，且只允许指定一个
    + **O_RDONLY：**只读模式
    + **O_WRONLY：**只写模式
    + **O_RDWR：**可读可写
    + **OEXEC：** 只执行打开
    + **O_SEARCH：** 只搜索打开(应用于目录)
  + 以下的常量是选用的，这些选项是用来和上面的必选项进行按位或起来作为oflags参数。
    + **O_APPEND：** 表示追加，如果原来文件里面有内容，则这次写入会写在文件的最末尾。 
    + **O_CREAT ：**表示如果指定文件不存在，则创建这个文件 
    + **O_EXCL：** 表示如果同时制定了O_CREAT，而文件已存在，则出错，同时返回 -1，并且修改 errno 的值。 
    + **O_TRUNC：** 表示截断，如果文件存在，并且以只写、读写方式打开，则将其长度截断为0。 
    + **O_NOCTTY：** 如果路径名指向终端设备，不要把这个设备用作控制终端。 
    + **O_NONBLOCK：** 如果路径名指向 FIFO/块文件/字符文件，则把文件的打开和后继 I/O设置为非阻塞模式（nonblocking mode） 
  + 以下三个常量同样是选用的，它们用于同步输入输出
    + **O_DSYNC：** 等待物理 I/O 结束后再 write。在不影响读取新写入的数据的前提下，不等待文件属性更新。 
    + **O_RSYNC：** read 等待所有写入同一区域的写操作完成后再进行 
    + **O_SYNC：** 等待物理 I/O 结束后再 write，包括更新文件属性的 I/O



### 1.open函数与fopen函数区别
从来源来分，这两者很好区分：

+ open函数是Unix下系统调用函数，操作成功返回的是文件描述符，操作失败返回的是-1,

+ fopen是ANSIC标准中C语言库函数，所以在不同的系统中调用不同的内核的API，返回的是一个指向文件结构的指针。

+ 同时open函数没有缓冲，fopen函数有缓冲，open函数一般和write配合使用，fopen函数一般和fwrite配合使用。

### 2.open函数与openat函数比较

#### 相同点

当传给函数的路径名是绝对路径时，二者无区别.（openat()自动忽略第一个参数fd）

#### 不同点

当传给函数的是相对路径时，如果openat()函数的第一个参数fd是常量AT_FDCWD时，则其后的第二个参数路径名是以当前工作目录为基址的；否则以fd指定的目录文件描述符为基址。

目录文件描述符的取得通常分为两步，先用opendir()函数获得对应的DIR结构的目录指针，再使用`int dirfd(DIR*)`函数将其转换成目录描述符，此时就可以作为openat()函数的第一个参数使用了。

#### 实例

1. 打开采用绝对路径表示的文件/home/leon/test.c,如果文件不存在就创建它。

```c
fd = open("/home/leon/test.c", O_RDWR | O_CREAT, 0640);
fd = openat(anything, "/home/leon/test.c", O_RDWR | O_CREAT, 0640);
```

2. 打开采用相对路径表示的文件

a.打开当前目录文件下的test.c

```c
fd = open("./test.c", O_RDWR | O_CREAT, 0640);
fd = openat( AT_FDCWD, O_RDWR | O_CREAT, 0640);
```

b.打开用户chalion家目录中的test.c文件,且此时你在自己的家目录

```c
DIR* dir_chalion = opendir(/home/chalion);
fd_chalion = dirfd(dir_chalion);
fd = openat(fd_chalion, "test.c" ,O_RDWR | O_CREAT, 0640);
```

## 三.函数create

可以调用create函数创建一个文件：

```c
#include <fcntl.h> 

int creat(const char *path, mode_t mode); 
//Returns: ﬁle descriptor opened for write-only if OK, −1 on error
```

此函数等效于`open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);`

creat的一个不足之处是它以只写方式打开所创建的文件。

## 四.函数close

调用close函数关闭一个打开的文件：

```c
#include <unistd.h> 

int close(int fd);
//Returns: 0 if OK, −1 on error
```

关闭一个文件时还会释放该进程加在该文件上的所有记录锁。

当一个进程终止时,内核自动关闭它所有的打开文件。很多程序都利用了这一功能而不显式地用close关闭打开文件。

## 五.函数lseek

每个打开文件都有一个与其相关联的“**当前文件偏移量**”( current file offset)。它通常是一个非负整数,用以度量从文件开始处计算的字节数。通常，读、写操作都是从当前文件偏移量开始，并使偏移量增加读写的字节数。在系统默认情况下，当打开一个文件时，除非指定O_APPEND项，否则当前偏移量被设置为0。

调用lseek显式地为一个打开文件设置偏移量：

```c
#include <unistd.h>

off_t lseek(int fd, off_t offset, int whence);
//Returns: new ﬁle offset if OK, −1 on error
```

对参数offet的解释与参数 whence的值有关。

- 若 whence是 SEEK_SET,则将该文件的偏移量设置为距文件开始处 offset个字节。
- 若 whence是 SEEK_CUR,则将该文件的偏移量设置为其当前值加 offset, offset可为正或负。
- 若 whence是 SEEK_END,则将该文件的偏移量设置为文件长度加 offset, offset可正可负。

若lseek成功执行，则返回新的文件偏移量，为此可以用下面方式确定打开文件的偏移量：

```c
off_t currpos;
currpos = lseek(fd, 0, SEEK_CUR);
```

lseek仅将当前的文件偏移量记录在内核中,它并不引起任何IO操作。然后,该偏移量用于下一个读或写操作。

文件偏移量可以大于文件的当前长度,在这种情况下,对该文件的下一次写将加长该文件,并在文件中构成一个空洞,这一点是允许的。位于文件中但没有写过的字节都被读为0

**文件中的空洞并不要求在磁盘上占用存储区**。具体处理方式与文件系统的实现有关,当定位到超出文件尾端之后写时,对于新写的数据需要分配磁盘块,但是对于原文件尾端和新开始写位置之间的部分则不需要分配磁盘块。

创建一个具有空洞的文件：

```c
#include "apue.h"
#include <fcntl.h>

char	buf1[] = "abcdefghij";
char	buf2[] = "ABCDEFGHIJ";

int
main(void)
{
	int		fd;

	// 创建文件
	if ((fd = creat("file.hole", FILE_MODE)) < 0)
		err_sys("creat error");

	// 写入a-j，此时文件偏移量为10
	if (write(fd, buf1, 10) != 10)
		err_sys("buf1 write error");
	/* offset now = 10 */

	// 改变文件偏移量到16384
	if (lseek(fd, 16384, SEEK_SET) == -1)
		err_sys("lseek error");
	/* offset now = 16384 */

	// 在新的文件偏移量下写入A-J
	if (write(fd, buf2, 10) != 10)
		err_sys("buf2 write error");
	/* offset now = 16394 */

	exit(0);
}
```

因为lseek使用的偏移量是用off_t类型表示的,所以允许具体实现根据各自特定的平台自行选择大小合适的数据类型。

## 六.函数read

调用read函数从打开文件中读数据

```c
#include <unistd.h> 

ssize_t read(int fd, void *buf, size_t nbytes); 
//Returns: number of bytes read, 0 if end of ﬁle, −1 on error
```

如read成功,则返回读到的字节数。如已到达文件的尾端,则返回0。

有多种情况可使实际读到的字节数少于要求读的字节数

- 读普通文件时,在读到要求字节数之前已到达了文件尾端。
- 当从终端设备读时,通常一次最多读一行。
- 当从网络读时,网络中的缓冲机制可能造成返回值小于所要求读的字节数
- 当从管道或FIFO读时,如若管道包含的字节少于所需的数量,那么read将只返回实际可用的字节数
- 当从某些面向记录的设备(如磁带)读时,一次最多返回一个记录。
- 当一信号造成中断,而已经读了部分数据量时。

## 七.函数write

调用write函数想打开文件写数据

```c
#include <unistd.h> 

ssize_t write(int fd, const void *buf, size_t nbytes); 
//Returns: number of bytes written if OK, −1 on error
```

其返回值通常与参数 nbytes的值相同,否则表示出错。 write出错的一个常见原因是磁盘已写满,或者超过了一个给定进程的文件长度限制

## 八.I/O的效率

BUFFSIZE 一般选取与磁盘块相同大小的字节数，大多数情况下4096

大多数文件系统为改善性能都采用某种预读( read ahead)技术。当检测到正进行顺序读取时,系统就试图读入比应用所要求的更多数据,并假想应用很快就会读这些数据。

## 九.文件共享







