
#ifndef MIDDLEWARE_SERVER_H
#define MIDDLEWARE_SERVER_H

#define MIDDLEWARE_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define MIDDLEWARE_BINDADDR_MAX 16

#define MIDDLEWARE_SERVERPORT        6379    /* TCP port */
#define MIDDLEWARE_MAX_QUERYBUF_LEN  10240
#define MIDDLEWARE_TCP_BACKLOG       511     /* TCP listen backlog */
#define MIDDLEWARE_IP_STR_LEN        INET6_ADDRSTRLEN
#define MIDDLEWARE_MIN_RESERVED_FDS     32
#define MIDDLEWARE_EVENTLOOP_FDSET_INCR (REDIS_MIN_RESERVED_FDS+96)
#define MIDDLEWARE_MAX_CLIENTS          10000

struct middlewareServer {
    // �����ļ��ľ���·��
    char *configfile;           /* Absolute config file path, or NULL */
    // �¼�״̬
    aeEventLoop *el;
    
    int port;                   /* TCP listening port */
    int tcp_backlog;            /* TCP listen() backlog */

    // ��ַ
    char *bindaddr[MIDDLEWARE_BINDADDR_MAX]; /* Addresses we should bind to */
    // ��ַ����
    int bindaddr_count;         /* Number of addresses in server.bindaddr[] */

    // һ���������������пͻ���״̬�ṹ  createClient�а�redisClient�ͻ�����ӵ��÷��������clients������  if (fd != -1) listAddNodeTail(server.clients,c);
    list *clients;               /* List of active clients */
    // �������������д��رյĿͻ���
    list *clients_to_close;     /* Clients to close asynchronously */

    // �Ƿ��� SO_KEEPALIVE ѡ��  tcp-keepalive ���ã�Ĭ�ϲ�����
    int tcpkeepalive;               /* Set SO_KEEPALIVE if non-zero. */

    int daemonize;                  /* True if running as a daemon */

    /* Logging */
    char *logfile;                  /* Path of log file */
    int syslog_enabled;             /* Is syslog enabled? */
    char *syslog_ident;             /* Syslog ident */
    int syslog_facility;            /* Syslog facility */

    /* Limits */
    int maxclients;                 /* Max number of simultaneous clients */
}

typedef struct middlewareClient {   //redisServer��redisClient��Ӧ
    // �׽���������
    int fd;
    char cip[REDIS_IP_STR_LEN];
    int cport;

    // ��ѯ������  Ĭ�Ͽռ��СREDIS_IOBUF_LEN����readQueryFromClient
    sds querybuf;//�������Ĳ������������argc��argv��   

    // ��������
    int argc;   //�ͻ������������processMultibulkBuffer   //ע��slowlog����¼32����������slowlogCreateEntry

    // ������������  resetClient->freeClientArgv���ͷſռ�
    robj **argv; //�ͻ������������processMultibulkBuffer  �����ռ�͸�ֵ��processMultibulkBuffer���ж��ٸ���������multibulklen���ʹ������ٸ�robj(redisObject)�洢�����Ľṹ


    // ��������ͣ���������Ƕ�������
    int reqtype;

    /* Response buffer */
    // �ظ�ƫ����
    int bufpos;
    // �ظ�������
    char buf[MIDDLEWARE_REPLY_CHUNK_BYTES];
}

#endif



