#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 8888
#define BACKLOG 5
#define BUF_SIZE 128

static int send_str(int sock, const char *str);
static int recv_str(int sock, char *buf, int max_len);
static int send_byte(int sock, const char *buf, int len);
static int recv_byte(int sock, char *buf, int len);
static int eat_byte(int sock, int len);

static int start_server(int port);
static int start_client(const char *addr, int port);

static char g_buf[BUF_SIZE];

int main(int argc, char **argv)
{
				int i, r, port;
				int server_mode = 0;
				char *addr;

				port = DEFAULT_PORT;
				server_mode = 0;
				addr = "127.0.0.1";

				for (i = 1; i < argc; i++) {
								if (strcmp(argv[i], "-s") == 0)
												server_mode = 1;
								else if (strcmp(argv[i], "-p") == 0) {
												i++;
												if (i >= argc) {
																printf("Error: must specify port number!\n");
																return -1;
												}

												port = atoi(argv[i]);
												if (port == 0) {
																printf("Error: wrong port number!\n");
																return -1;
												}
								} else {
												addr = argv[i];
								}
				}

				if (server_mode) {
								r = start_server(port);
				} else {
								r = start_client(addr, port);
				}

				return r;
}

int start_server(int port)
{
				int r, val;
				int sock_listen, sock;
				pid_t pid;
				char prompt[25];
				socklen_t socklen;

				sock_listen = socket(AF_INET, SOCK_STREAM, 0);
				if (sock_listen < 0) {
								perror("start_server:socket()");
								return -1;
				}

				val = 1;
				setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

				struct sockaddr_in host_addr, peer_addr;
				memset(&host_addr, 0, sizeof(host_addr));
				host_addr.sin_family = AF_INET;
				host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
				host_addr.sin_port = htons(port);
				r = bind(sock_listen, (struct sockaddr *)&host_addr, sizeof(host_addr));
				if (r < 0) {
								perror("start_server:bind()");
								return -1;
				}

				r = listen(sock_listen, BACKLOG);
				if (r < 0) {
								perror("start_server:listen()");
								close(sock_listen);
								return -1;
				}
				printf("Server start listening on port:%d.\n", port);

				struct sigaction sa;
				memset(&sa, 0, sizeof(sa));
				sa.sa_handler = SIG_DFL;
				sa.sa_flags = SA_NOCLDWAIT;
				sigaction(SIGCHLD, &sa, NULL);

				for (;;) {
								socklen = sizeof(peer_addr);
								sock = accept(sock_listen, (struct sockaddr *)&peer_addr,
												&socklen);
								if (sock < 0) {
												perror("start_server:accept()");
												continue;
								}

								pid = fork();
								if (pid != 0) {
												close(sock);
												continue;
								}

								sprintf(prompt, "%s:%d", inet_ntoa(peer_addr.sin_addr),
												ntohs(peer_addr.sin_port));
								printf("Accept remote %s,pid=%d.\n", prompt, getpid());

								for (;;) {
												// FIXME please use socketpair
												if (send_str(sock, "OK") <= 0 ||
																recv_str(sock,g_buf, sizeof(g_buf)) <= 0) {
																printf("%s error or disconnected, "
																				"socket is closing...\n", prompt);
																close(sock);
																return 0;
												}
												printf("%s> %s\n", prompt, g_buf);
								}
				}
				return 0;
}

int start_client(const char* addr, int port)
{
				int r ;
				int sock;
				struct sockaddr_in server_addr;
				if((sock=socket(AF_INET,SOCK_STREAM,0))<0){
								perror("start_client:socket()");
								return -1;
				}

				memset(&server_addr,0,sizeof(server_addr));
				server_addr.sin_family=AF_INET;
				server_addr.sin_port=htons(port);
				if(!inet_aton(addr,&server_addr.sin_addr)){
								perror("start_client:inet_aton()");
								return -1;
				}

				r=connect(sock,(struct sockaddr*)&server_addr,sizeof(server_addr));
				if(r<0){
								perror("start_client:connect()");
								close(sock);
								return -1;
				}

				while(1){
								/*接收一个字符串*/
								if(recv_str(sock,g_buf,sizeof(g_buf))<=0)
												break;
								/*显示收到的字符串*/
								printf(">%s\n",g_buf);
								/*让用户输入一行数据*/
								fgets(g_buf,sizeof(g_buf),stdin);
								g_buf[strlen(g_buf)-1]='\0';

								if(send_str(sock,g_buf)<=0)
												break;
				}
				close(sock);
				return 0;
}

/*发送一个字符串*/
int send_str(int sock,const char* str)
{
				int byte;
				int len;
				int len_send;
				/*首先发送一个整数，表示一个字符串的长度*/
				len=strlen(str);
				len_send=htonl(len);
				byte=send_byte(sock,(char*)&len_send,sizeof(len_send));
				if(byte<0)
								return -1;
				/*然后发送字符串本身*/
				byte=send_byte(sock,str,len);
				if(byte<0)
								return -1;
}

/*接收字符串*/
int recv_str(int sock,char* buf,int max_len)
{
				int byte;
				int len;
				int len_recv;
				int len_diff=0;
				/*首先接收一个整数，这个整数表示字符串长度,注意接收的是字符流*/
				byte=recv_byte(sock,(char*)&len_recv,sizeof(len_recv));
				if(byte<0)
								return -1;
				len=ntohl(len_recv);    /*转化成int*/
				if(len>max_len){
								len_diff=len-max_len;
								len=max_len;
				}
				/*然后接收字符串*/
				byte=recv_byte(sock,buf,len);
				if(byte<=0)
								return -1;
				buf[byte]='\0';
				/*如果实际接收的字符数小于字符串的长度，则消耗掉多于的字符*/
				if(len_diff>0)
								eat_byte(sock,len_diff);
				return byte;
}

/*可靠发送 len 个字节*/
int send_byte(int sock,const char* buf,int len)
{
				int rc;
				int byte;
				for(byte=0;byte<len;byte+=rc){
								rc=send(sock,buf+byte,len-byte,MSG_NOSIGNAL);
								if(rc<0 && errno !=EINTR){
												byte=-1;
												break;
								}
				}
				return byte;
}

/*可靠接收 len 个字节*/
int recv_byte(int sock,char* buf,int len)
{
				int rc;
				int byte;
				for(byte=0;byte<len;byte+=rc){
								rc=recv(sock,buf+byte,len-byte,MSG_NOSIGNAL);
								if(rc==0)
												break;
								if(rc<0 && errno!=EINTR){
												byte=-1;
												break;
								}
				}
				return byte;
}

/*消耗 len 个字节*/
int eat_byte(int sock,int len)
{
				int rc;
				int byte;
				char buf[32];
				for(byte=0;len-byte>32;byte+=rc){
								rc=recv(sock,buf,32,MSG_NOSIGNAL);
								if(rc==0)
												break;
								if(rc<0 && errno!=EINTR)
												return -1;
				}
				if(rc!=0)
								byte+=recv_byte(sock,buf,len-byte);
				return byte;
}


/*
  在这个例子中，要注意学会：设置socket的可重用
	设计传输协议，怎样实现
	模块化的结构
	设置sigaction，来避免僵尸进程的出现
	怎样可靠的接收/发送 len 长度的字节
	在这个例子中，使用了自定义的协议来传输一个字符串，即先发送4B，表示字符串的长度，然后再是字符串本身。
	这个协议是必要的，因为“在数据流形式的传输中，接收端无法分辨字符串在哪里结束。”  @@@@@@@@@@@@@@@@@@@@
	要非常注意这一点。
*/
