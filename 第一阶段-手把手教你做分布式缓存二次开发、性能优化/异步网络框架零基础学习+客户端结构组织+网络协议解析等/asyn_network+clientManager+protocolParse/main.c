#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#include "ae.h"
#include "anet.h"

#define REDIS_SERVERPORT        6379    /* TCP port */
#define REDIS_MAX_QUERYBUF_LEN  10240
#define REDIS_TCP_BACKLOG       511     /* TCP listen backlog */
#define REDIS_IP_STR_LEN        INET6_ADDRSTRLEN
#define REDIS_MIN_RESERVED_FDS     32
#define REDIS_EVENTLOOP_FDSET_INCR (REDIS_MIN_RESERVED_FDS+96)
#define REDIS_MAX_CLIENTS          10000

//������Ϣ
char g_neterror_buf[1024];

//epoll�¼�ѭ������
aeEventLoop *g_epoll_loop = NULL;

/*
��ʱʱ�䵽
*/
int MainTimerExpire(struct aeEventLoop *eventLoop, long long id, void *clientData)
{
    printf("MainTimerExpire\n");

    //15����ٴ�ִ�иú���,������ﷵ��0����ʱ��ֻ��ִ��һ�Σ����ش���0�����������Զ�ʱ��
    return 15000;
}

void MainCloseFd(aeEventLoop *el, int fd)
{

    //ɾ����㣬�ر��ļ�
    aeDeleteFileEvent(el, fd, AE_READABLE);
    close(fd);
}

/*
������fd�����ݶ�д
*/
void MainReadFromClient(aeEventLoop *el, int fd, void *privdata, int mask)
{
    char buffer[REDIS_MAX_QUERYBUF_LEN] = { 0 };
    int nread, nwrite;
    
    char client_ip[REDIS_IP_STR_LEN];
    int client_port;
    anetPeerToString(fd, client_ip, REDIS_IP_STR_LEN, &client_port);

    
    nread = read(fd, buffer, REDIS_MAX_QUERYBUF_LEN);
    /* ��fd��Ӧ��Э��ջbufû�����ݿɶ� */
    if (nread == -1 && errno == EAGAIN) return; /* No more data ready. */

    //˵���ͻ��˹ر�������
    if(nread <= 0) { 
        printf("I/O error reading from node link: %s",
                (nread == 0) ? "connection closed" : strerror(errno));
        MainCloseFd(el, fd);
    } else { //��������������Ѷ��������ݷ��ظ��ͻ��ˣ�Ҳ���ǿͻ��˻��յ����͵�����
        printf("recv from client %s:%d, data:%s\r\n", client_ip, client_port, buffer);
        nwrite = write(fd, buffer, REDIS_MAX_QUERYBUF_LEN);
        if(nwrite == -1) //д�쳣�ˣ������ǿͻ��˹ر������ӣ�Ҳ�����ǿͻ��˽��̹��˵�
            MainCloseFd(el, fd);
    }
}

/*
����ͻ�������
*/
void MainAcceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask)
{
    int cfd, port;
    char ip_addr[128] = { 0 };
    cfd = anetTcpAccept(g_neterror_buf, fd, ip_addr, sizeof(ip_addr), &port);
    if (cfd == ANET_ERR) { //accept�����쳣
        if (errno != EWOULDBLOCK)
            printf("Accepting client connection: %s", g_neterror_buf);
        return;
    }
    printf("client %s:%d Connected\n", ip_addr, port);
    
    // ������
    anetNonBlock(NULL, cfd);
    // ���� Nagle �㷨
    anetEnableTcpNoDelay(NULL, cfd);
    
    //��accept���ص����׽���cfdע�ᵽepoll�¼����У������׽��ֹ�עAE_READABLE���¼�������ͻ��������ݹ�������
    //�������ص�����MainReadFromClient
    if(aeCreateFileEvent(el, cfd, AE_READABLE,
        MainReadFromClient, NULL) == AE_ERR ) {
        fprintf(stderr, "client connect fail: %d\n", cfd);
        close(cfd);
    }
}

void MainPriorityRun(struct aeEventLoop *eventLoop) {
    printf("I run befor all other epoll event\n");
}
int main()
{

    printf("process Start begin\n");

    //��ʼ��EPOLL�¼�������״̬
    g_epoll_loop = aeCreateEventLoop(REDIS_MAX_CLIENTS + REDIS_EVENTLOOP_FDSET_INCR);

    //�����׽��ֲ�bind
    int sd = anetTcpServer(g_neterror_buf, REDIS_SERVERPORT, NULL, REDIS_TCP_BACKLOG);
    if( ANET_ERR == sd ) {
        fprintf(stderr, "Open port %d error: %s\n", REDIS_SERVERPORT, g_neterror_buf);
        exit(1);
    }

    //����socket bind��Ӧ��fdΪ������
    anetNonBlock(NULL, sd);
    //ĳЩ����£�����˽����˳��󣬻��м����˿�timwait״̬�����ӣ���ʱ������������̣�����ʾerror:98��Address already in use���Ϳ����ø���������������⡣
    anetSetReuseAddr(NULL, sd);
    if(aeCreateFileEvent(g_epoll_loop, sd, AE_READABLE,  MainAcceptTcpHandler, NULL) == AE_ERR ) {
        fprintf(stderr, "aeCreateFileEvent failed");
        exit(1);
    }
    
    //���ö�ʱ��
    aeCreateTimeEvent(g_epoll_loop, 1, MainTimerExpire, NULL, NULL);
    //��aeMainѭ���У��������иûص�
    aeSetBeforeSleepProc(g_epoll_loop, MainPriorityRun);
    //�����¼�ѭ��
    aeMain(g_epoll_loop);

    //ɾ���¼�ѭ��
    aeDeleteEventLoop(g_epoll_loop);

    printf("process End\n");

    return 0;
}

