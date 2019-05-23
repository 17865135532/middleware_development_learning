#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h> 
#include <fcntl.h> 
#include <errno.h>


void error_handling(char *message);

/*
 * �� fd ����Ϊ������ģʽ��O_NONBLOCK��
 */
int anetNonBlock(int fd)
{
    int flags;

    /* Set the socket non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        printf("fcntl(F_GETFL): %s", strerror(errno));
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        printf("fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return -1;
    }
    return 0;
}

/*
 * ���� socket ������� buffer �ֽ���
 */
int anetSetSendBuffer(int fd, int buffsize)
{
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsize, sizeof(buffsize)) == -1)
    {
        printf("set send buf failed\r\n");
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[]){
	int sd;
	int fd;

	struct sockaddr_in serv_addr;
	struct sockaddr_in clnt_addr;
	socklen_t clnt_addr_size;
	int str_len;
	int write_len = 1024 * 1024 * 10;
	int ret_len = 0;
	char *write_str = malloc(write_len);
	
	char message[1000];
	if(argc != 2){
		printf("Usage : %s <port>\n",argv[0]);
		exit(1);
	}
	sd = socket(PF_INET,SOCK_STREAM,0);
	if(sd == -1){
		error_handling("socket() error");
	}

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_addr.sin_port=htons(atoi(argv[1]));

	if(bind(sd,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1){
		error_handling("bind() error");
	}

	if(listen(sd,5) == -1){
		error_handling("listen() error");
	}
	anetNonBlock(sd);
    
	clnt_addr_size = sizeof(clnt_addr);
	printf("begin accept\r\n");
    while(1) {
        fd = accept(sd,(struct sockaddr *)&clnt_addr,&clnt_addr_size);
        if (fd == -1) { //acceptϵͳ���ò��ܼ�⵽��û�п������ӣ�accept��������
            if (errno == EINTR)
                continue;
            else {
                //printf("accept: %s", strerror(errno)); 
                //�����Ƿ�����(������anetNonBlock)��ʽ��acceptϵͳ����û�л�ȡ�������ӣ����ֱ�ӷ��أ����������������

                printf("accept return \r\n");
                //�������������ʱ�������Ƿ�������ʽ���û�̬�ͻ᲻ͣ��accept����ϵͳ���ã������Ĵ���CPU
                //1S�����accept
                sleep(1); 
                continue; //����ȥ��ȡ�����ӣ�ֱ����ȡ��������fd��ͨ��break�����˳�whileѭ��
            }
        }
        break; //��ȡ�������ӣ��˳�ѭ��
    }
    printf("accept successful from client\r\n\r\n");

    printf("begin recv message\r\n");
    anetNonBlock(fd);
    while(1) {
        str_len = read(fd, message, sizeof(message)-1);
        if(str_len == -1) {
            printf("read return \r\n");
            sleep(1);//1s�����read
            continue;
        }
        break;
    }
    printf("recv message:%s from client\r\n\r\n", message);


    anetSetSendBuffer(fd, 10*1024);
	printf("begin write message\r\n");
	//fd����Ϊnoblock�󣬼�ʹҪд�����ݱ�sendbuf��ܶ࣬writeҲ����������
	ret_len= write(fd, write_str, write_len);
	printf("end write message, write len:%d\r\n", ret_len);

	sleep(1);
	close(fd);
	close(sd);

	return 0;

}

void error_handling(char *message){

	fputs(message,stderr);
	fputs("\n",stderr);
	exit(1);
}
