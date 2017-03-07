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
int start_client(const char* addr, int port);

static char g_buf[BUF_SIZE];

int main(int argc, char **argv)
{
				int i, r, port;
				char *addr;
				addr = "127.0.0.1";
				port = 8888;
				for (i = 1; i < argc; i++) {
								if (strcmp(argv[i], "-p") == 0) {
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

				r = start_client(addr, port);
				return r;
}

int start_client(const char* addr, int port)
{
				int r ;
				int sock;
				struct sockaddr_in server_addr;
				if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
								perror("start_client: socket()");
								return -1;
				}

				memset(&server_addr, 0, sizeof(server_addr));
				server_addr.sin_family = AF_INET;
				server_addr.sin_port = htons(port);
				if (!inet_aton(addr, &server_addr.sin_addr)) {
								perror("start_client: inet_aton()");
								return -1;
				}

				r = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
				if (r < 0) {
								perror("start_client: connect()");
								close(sock);
								return -1;
				}

				for(;;) {
								if (recv_str(sock, g_buf, sizeof(g_buf)) <= 0)
												break;

								printf(">%s\n", g_buf);

								fgets(g_buf, sizeof(g_buf), stdin);
								g_buf[strlen(g_buf) - 1] = '\0';

								if (send_str(sock, g_buf) <= 0)
												break;
				}
				close(sock);
				return 0;
}

int send_str(int sock, const char* str)
{
				int byte;
				int len;
				int len_send;

				len = strlen(str);
				len_send = htonl(len);
				byte = send_byte(sock, (char *)&len_send, sizeof(len_send));
				if (byte < 0)
								return -1;

				byte = send_byte(sock, str, len);
				if (byte < 0)
								return -1;

				return 0;
}

int recv_str(int sock, char* buf, int max_len)
{
				int byte;
				int len;
				int len_recv;
				int len_diff = 0;

				byte = recv_byte(sock, (char *)&len_recv, sizeof(len_recv));
				if (byte < 0)
								return -1;
				len = ntohl(len_recv);
				if (len > max_len) {
								len_diff = len - max_len;
								len = max_len;
				}

				byte = recv_byte(sock, buf, len);
				if (byte <= 0)
								return -1;
				buf[byte] = '\0';

				if (len_diff > 0)
								eat_byte(sock, len_diff);
				return byte;
}

int send_byte(int sock,const char* buf,int len)
{
				int rc;
				int byte;
				for (byte = 0; byte < len; byte += rc) {
								rc = send(sock, buf + byte, len - byte, MSG_NOSIGNAL);
								if (rc < 0 && errno != EINTR) {
												byte = -1;
												break;
								}
				}
				return byte;
}

int recv_byte(int sock, char *buf, int len)
{
				int rc;
				int byte;
				for (byte = 0; byte < len; byte += rc) {
								rc = recv(sock, buf + byte, len - byte, MSG_NOSIGNAL);
								if (rc == 0)
												break;
								if (rc < 0 && errno != EINTR) {
												byte = -1;
												break;
								}
				}
				return byte;
}

int eat_byte(int sock,int len)
{
				int rc;
				int byte;
				char buf[32];
				for (byte = 0; len - byte > 32; byte += rc) {
								rc = recv(sock, buf, 32, MSG_NOSIGNAL);
								if (rc == 0)
												break;
								if (rc < 0 && errno != EINTR)
												return -1;
				}
				if (rc != 0)
								byte += recv_byte(sock, buf, len - byte);
				return byte;
}
