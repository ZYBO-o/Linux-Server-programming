#define _GNU_SOURCE 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>

#define USER_LIMIT 5        //最大用户数量  
#define BUFFER_SIZE 64      //读缓冲区的大小
#define FD_LIMIT 65535      //文件描述符数量限制

//客户数据
struct client_data
{
    sockaddr_in address;            //客户端socket地址
    char* write_buf;                //待写到客户端的数据的位置
    char buf[ BUFFER_SIZE ];        //从客户端读入的数据
};

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", basename( argv[0] ) );
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    //创建 FD_LIMIT 个用户元素？
    client_data* users = new client_data[FD_LIMIT];
    //创建 USER_LIMIT + 1 个poll事件数组
    pollfd fds[USER_LIMIT + 1];
    //连接数目
    int user_counter = 0;
    //先初始化为-1
    for( int i = 1; i <= USER_LIMIT; ++i ){
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    //数组第一个注册的是 listened 文件描述符
    fds[0].fd = listenfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while( 1 )
    {   //进行系统调用
        ret = poll( fds, user_counter + 1, -1 );
        if ( ret < 0 ){
            printf( "poll failure\n" );
            break;
        }
        //遍历结束后返回的 事件数组
        for( int i = 0; i < user_counter + 1; ++i ){
            //如果是 listenfd 描述符 并且是 读事件
            if( ( fds[i].fd == listenfd ) && ( fds[i].revents & POLLIN ) ) {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                //进行连接
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                if ( connfd < 0 ){
                    printf( "errno is: %d\n", errno );
                    continue;
                }
                //如果用户数超过极限，则报错，关闭当前的 socket
                if( user_counter >= USER_LIMIT ){
                    const char* info = "too many users\n";
                    printf( "%s", info );
                    send( connfd, info, strlen( info ), 0 );
                    close( connfd );
                    continue;
                }
                //用户数目 + 1
                user_counter++;
                //文件描述符对应的用户数组进行填写信息： 客户端socket地址
                users[connfd].address = client_address;
                //设置为非阻塞
                setnonblocking( connfd );
                //注册新用户对应的 poll 事件
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                printf( "comes a new user, now have %d users\n", user_counter );
            }
            //如果是 错误事件
            else if( fds[i].revents & POLLERR )
            {
                //输入错误信息
                printf( "get an error from %d\n", fds[i].fd );
                char errors[ 100 ];
                memset( errors, '\0', 100 );
                socklen_t length = sizeof( errors );
                if( getsockopt( fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length ) < 0 ){
                    printf( "get socket option failed\n" );
                }
                continue;
            }
            //如果是对方关闭链接的事件
            else if( fds[i].revents & POLLRDHUP )
            {   //将最后一个 用户信息 赋给 关闭链接 的用户信息
                users[fds[i].fd] = users[fds[user_counter].fd];
                //关闭 原来对应的 socket 
                close( fds[i].fd );
                //传递新的 socket
                fds[i] = fds[user_counter];
                //循环退回一步，这样用来重新检查
                i--;
                //用户数目 - 1
                user_counter--;
                //输出退出信息
                printf( "a client left\n" );
            }
            //如果对应的是读事件
            else if( fds[i].revents & POLLIN )
            {   //定义一个 socket 接受 读事件的socket
                int connfd = fds[i].fd;
                memset( users[connfd].buf, '\0', BUFFER_SIZE );
                //将文件描述符中的数据 读到 用户客户端数据处
                ret = recv( connfd, users[connfd].buf, BUFFER_SIZE-1, 0 );
                printf( "get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd );
                //缓冲区数据被完全接收
                if( ret < 0 ){
                    if( errno != EAGAIN )
                    {
                        close( connfd );
                        //删除当前的用户
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if( ret == 0 )
                {
                    printf( "code should not come to here\n" );
                }
                else
                {
                    //将数据发送给通知其他socket
                    for( int j = 1; j <= user_counter; ++j )
                    {
                        if( fds[j].fd == connfd ){
                            continue;
                        }
                        
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        //其他用户需要写的数据
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            }
            //如果是写事件
            else if( fds[i].revents & POLLOUT )
            {
                int connfd = fds[i].fd;
                //如果缓冲区为空，则继续
                if( ! users[connfd].write_buf ) {
                    continue;
                }
                //将数据发送给对应的用户
                ret = send( connfd, users[connfd].write_buf, strlen( users[connfd].write_buf ), 0 );
                //重新调整用户信息
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }

    delete [] users;
    close( listenfd );
    return 0;
}
