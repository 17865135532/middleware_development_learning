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
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>

/* Log levels */
#define REDIS_DEBUG 0
#define REDIS_VERBOSE 1
#define REDIS_NOTICE 2
#define REDIS_WARNING 3
#define REDIS_LOG_RAW (1<<10) /* Modifier to log without timestamp */
#define REDIS_DEFAULT_VERBOSITY     REDIS_NOTICE
#define REDIS_DEFAULT_SYSLOG_IDENT  "redis-syslog"

#define REDIS_MAX_LOGMSG_LEN    1024 /* Default maximum length of syslog messages */

struct server {
    /* Logging */
    char logfile[100];                  /* Path of log file */

    // ��־�ɼ���,������־��ӡ����ֻ��levelС�ڸü���ĲŻ��ӡ����redisLogRaw
    int verbosity;                  /* Loglevel in redis.conf */

    int syslog_enabled;             /* Is syslog enabled? */
    char *syslog_ident;             /* Syslog ident */
    int syslog_facility;            /* Syslog facility */
};

struct server server;

/* Low level logging. To use only for very big messages, otherwise
 * redisLog() is to prefer. */
//���ָ����logfile�ļ����������logfile�����û�������ն˴�ӡ��ͬʱ���������syslog�첽��־д����������syslogָ��������
void redisLogRaw(int level, const char *msg) { //��־�����Ǵ��ļ���д�룬Ȼ��رգ�����Ӧ�ÿ����Ż���ֻ���һ�μ���
    const int syslogLevelMap[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING };
    const char *c = ".-*#";
    FILE *fp;
    char buf[64];
    int rawmode = (level & REDIS_LOG_RAW);
    //���û������logfile�����������׼���stdout�������¼��logfile�ļ�
    int log_to_stdout = server.logfile[0] == '\0';

    level &= 0xff; /* clear flags */

    //������־���𣬿����Ƿ���Ҫ��ӡ��ֻ�д��ڵ���verbosity����־����Ż����
    if (level < server.verbosity) return; 

    //���û������logfile�����������׼���stdout�������¼��logfile�ļ�
    fp = log_to_stdout ? stdout : fopen(server.logfile,"a"); //ע����׷�ӷ�ʽд��
    if (!fp) return;

    if (rawmode) {
        fprintf(fp,"%s",msg);
    } else {
        int off;
        struct timeval tv;

        gettimeofday(&tv,NULL);
        off = strftime(buf,sizeof(buf),"%d %b %H:%M:%S.",localtime(&tv.tv_sec));
        snprintf(buf+off,sizeof(buf)-off,"%03d",(int)tv.tv_usec/1000);
        fprintf(fp,"[%d] %s %c %s\n",(int)getpid(),buf,c[level],msg);
    }
    //��֤ÿ����־��ˢ�̣������־���ܴ�д�ļ������������м�ע��
    fflush(fp);

    if (!log_to_stdout) fclose(fp);
    if (server.syslog_enabled) syslog(syslogLevelMap[level], "%s", msg);
}

/* Like redisLogRaw() but with printf-alike support. This is the function that
 * is used across the code. The raw version is only used in order to dump
 * the INFO output on crash. */
void redisLog(int level, const char *fmt, ...) {
    va_list ap;
    char msg[REDIS_MAX_LOGMSG_LEN];

    if ((level&0xff) < server.verbosity) return;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    redisLogRaw(level,msg);
}

//ֻ������ļ�
int main1(int argc, char **argv) {
    strcpy(server.logfile, "./log");
    
    server.verbosity = REDIS_NOTICE;

    //����syslog�첽��־д
    server.syslog_enabled = false;
    
    redisLog(REDIS_DEBUG, "test0,REDIS_DEBUG level output to stdout"); //debug�������notice���𣬲���д���ļ�
    redisLog(REDIS_WARNING, "test0,REDIS_WARNING level output to stdout"); //warning�������notice���𣬻�д���ļ�
    return 0;
}

//ֻ������ն�
int main2(int argc, char **argv) {
    memset(server.logfile, 0, sizeof(server.logfile));
    
    server.verbosity = REDIS_NOTICE;
    
    server.syslog_enabled = false;

    redisLog(REDIS_DEBUG, "test1,REDIS_DEBUG level output to stdout"); //debug�������notice���𣬲������ն���ʾ
    redisLog(REDIS_WARNING, "test1,REDIS_WARNING level output to stdout"); //warning�������notice���𣬻�������ն�
    return 0;
}

/*
rsyslog��������:
1. /etc/rsyslog.conf�ļ��м�����������:
   $IncludeConfig /etc/rsyslog.d/*.conf
2. ��/etc/rsyslog.d/�����һ��xxxxx.conf(xxxxx��������ָ������ú���ĳ��򱣳�һ�¶�Ӧ)
    $umask 0000
    $DirCreateMode  0755
    $FileCreateMode 0644
    $FileOwner syslogtest
    $FileGroup syslogtest
    $template clientlog,"/home/testlog/%programname%/log/log.%$year%%$month%%$day%%$hour%"

    $MainMsgQueueType LinkedList
    $MainMsgQueueSize 200000
    $MainMsgQueueDiscardSeverity 2
    $MainMsgQueueDiscardMark 180000
    $MainMsgQueueTimeoutEnqueue 1
    $ActionQueueSize 200000
    $ActionQueueDiscardSeverity 2
    $ActionQueueDiscardMark 180000
    $ActionQueueTimeoutEnqueue 1

    local3.* -?clientlog
3. ����rsyslog
   service rsyslog restart (ע�⣬ǧ����ɱ���̣�Ȼ��ͨ��/usr/sbin/rsyslogd -n������������rsyslog��������쳣)
*/
//������ļ�������¼��syslog������־��
int main(int argc, char **argv) {
    strcpy(server.logfile, "./log");
    
    server.verbosity = REDIS_NOTICE;
    
    server.syslog_enabled = true;

        // ���� syslog
    if (server.syslog_enabled) {
        //2019-06-01T18:22:30.494360+08:00 localhost redis-syslog[24855]: test2,REDIS_WARNING level output to stdout
        //��Ӧsyslog����е�redis-syslog�ַ�����ͬʱ��Ӧ�����ļ��е�%programname%   �������ļ�$template clientlog,"/home/testlog/%programname%/log/log.%$year%%$month%%$day%%$hour%"
        server.syslog_ident = REDIS_DEFAULT_SYSLOG_IDENT;
        server.syslog_facility = LOG_LOCAL3; //�������ļ��е�local3.* -?clientlog��Ӧ
        openlog(server.syslog_ident, LOG_PID | LOG_NDELAY | LOG_NOWAIT,
            server.syslog_facility);
    }

    redisLog(REDIS_DEBUG, "test2,REDIS_DEBUG level output to stdout"); //debug�������notice���𣬲����¼��./log�ļ���syslog��
    redisLog(REDIS_WARNING, "test2,REDIS_WARNING level output to stdout"); //warning�������notice���𣬻��¼��./log�ļ���syslog��
    return 0;
}


