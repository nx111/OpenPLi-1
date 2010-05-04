// pmttrans.c

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

#define PMT_SERVER_SOCKET "/tmp/.listen.camd.socket"
#define PMT_SERVER_SOCKET_BACKUP "/tmp/listen.camd.socket"

static int pmt_socket_backuped = 0;

static unsigned char pmt_buffer[1024];
static int pmt_len = 0;

static pthread_mutex_t pmt_distribute_lock;
static pthread_cond_t new_pmt_arrived;

static pthread_mutex_t connections_table_lock;
#define SIZE_OF_CONNECTIONS_TABLE	10
static int client_fd[SIZE_OF_CONNECTIONS_TABLE];

void utils_dump(char *label, unsigned char *data, int len)
{
	int i;
	printf(label);
	for(i = 0; i < len; i++) {
		printf("%02X ", data[i]);
        }
	printf(" [len=%02X]\n", len);
}

int is_pmt_valid(unsigned char *pmt, int pmt_size)
{
	if (pmt_size < 31)
		return 0;
	if (memcmp(pmt, "\x9f\x80\x32\x82", 4) == 0)
		return 1;
	return 0;
}

struct PMT_ENTRY
{
	unsigned char stream_type;
	unsigned char pid[2];
	unsigned char ca_descriptor_len[2];
	unsigned char *ca_descriptor;
};

int convert_pmt(unsigned char *original_pmt, int pmt_size, unsigned char *new_pmt)
{
	if (is_pmt_valid(original_pmt, pmt_size) == 0)
	{
		memcpy(new_pmt, original_pmt, pmt_size);
		return pmt_size;
	}

	struct PMT_ENTRY pmt_entry[10];
	int pmt_entry_count = 0;

	int len = (original_pmt[10] << 8) + original_pmt[11];
	int base_info_len = 12 + len;
	int wp = base_info_len;

	while (wp < pmt_size)
	{
		memcpy(&(pmt_entry[pmt_entry_count]), original_pmt + wp, 5);
		len = (pmt_entry[pmt_entry_count].ca_descriptor_len[0] << 8) + pmt_entry[pmt_entry_count].ca_descriptor_len[1];
		wp += 5;
		if (len == 0)
			pmt_entry[pmt_entry_count].ca_descriptor = NULL;
		else
		{
			pmt_entry[pmt_entry_count].ca_descriptor = original_pmt + wp;
			wp += len;
		}
		pmt_entry_count++;
	}

	int i = 0;
	int bp = 0, np = 0;
	while (i < pmt_entry_count)
	{
		memcpy(new_pmt + bp, original_pmt, base_info_len);
		np = base_info_len;

		do
		{
			memcpy(new_pmt + bp + np, &(pmt_entry[i]), 5);
			np += 5;
			len = (pmt_entry[i].ca_descriptor_len[0] << 8) + pmt_entry[i].ca_descriptor_len[1];
			if (len > 0)
			{
				memcpy(new_pmt + bp + np, pmt_entry[i].ca_descriptor, len);
				np += len;
			}

			if (i == pmt_entry_count - 1)
				break;

			if (pmt_entry[i + 1].ca_descriptor == NULL)
				i++;
			else
			{
				if (pmt_entry[i].ca_descriptor == NULL)
					break;
				else
				{
					if (len == ((pmt_entry[i + 1].ca_descriptor_len[0] << 8) + pmt_entry[i + 1].ca_descriptor_len[1]) &&
						memcmp(pmt_entry[i].ca_descriptor, pmt_entry[i + 1].ca_descriptor, len) == 0)
						i++;
					else
						break;
				}
			}
		} while (1);

		len = np - 6;
		new_pmt[bp + 4] = (len >> 8) & 0xFF;
		new_pmt[bp + 5] = len & 0xFF;
		i++;
		if (bp == 0)
		{
			if (i == pmt_entry_count)
				new_pmt[bp + 6] = 3;
			else
				new_pmt[bp + 6] = 1;
		}
		else
		{
			if (i == pmt_entry_count)
				new_pmt[bp + 6] = 2;
			else
				new_pmt[bp + 6] = 0;
		}
		bp += np;
	}

	return bp;
}

void add_to_connections_table(int clientfd)
{
	int i;
	pthread_mutex_lock(&connections_table_lock);
	for (i = 0; i < SIZE_OF_CONNECTIONS_TABLE; i++)
	{
		if (client_fd[i] == -1)
		{
			client_fd[i] = clientfd;
			break;
		}
	}
	pthread_mutex_unlock(&connections_table_lock);
}

void *new_pmt_server_proc(void *pdata)
{
	int newpmtfd;
	struct sockaddr_un saddr;

	printf("Trying start new PMT listen socket...\n");

	/* socket init */
	saddr.sun_family = AF_UNIX;
	strncpy(saddr.sun_path, PMT_SERVER_SOCKET, 107);
	saddr.sun_path[107] = '\0';
	unlink(saddr.sun_path);
	if((newpmtfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		printf("socket error (errno=%d)\n", errno);
		perror("");
		exit(1);
	}
	if (bind(newpmtfd, (struct sockaddr *)&saddr,
		sizeof(saddr.sun_family)+strlen(saddr.sun_path) ) < 0) {
		printf("socket bind error (errno=%d)\n", errno);
		perror("");
		exit(2);
	}
	if (listen(newpmtfd, 5) < 0) {
		printf("socket listen error (errno=%d)\n", errno);
		perror("");
		exit(3);
	}

	pthread_mutex_init(&connections_table_lock, NULL);

	while(1)
	{
		struct sockaddr_in client_addr;
		socklen_t length = sizeof(client_addr);

		int clientfd = accept(newpmtfd,(struct sockaddr*)&client_addr,&length);
		if (clientfd < 0)
		{
			printf("PMT server accept error (errno=%d)\n", errno);
			perror("");
			break;
		}

		pthread_mutex_lock(&pmt_distribute_lock);
		if (send(clientfd, pmt_buffer, pmt_len, 0) == pmt_len)
		{
			add_to_connections_table(clientfd);
			printf("A client connected.\n");
		}
		pthread_mutex_unlock(&pmt_distribute_lock);
	}

	pthread_exit(NULL);
}

void *pmt_distribute_proc(void *pdata)
{
	int i, len;

	pthread_mutex_lock(&pmt_distribute_lock);

	while(pthread_cond_wait(&new_pmt_arrived, &pmt_distribute_lock) == 0)
	{
		pthread_mutex_lock(&connections_table_lock);
		for (i = 0; i < SIZE_OF_CONNECTIONS_TABLE; i++)
		{
			if (client_fd[i] != -1)
			{
				printf("sending, client fd is %d\n", client_fd[i]);
				len = send(client_fd[i], pmt_buffer, pmt_len, MSG_NOSIGNAL);
				printf("sended, return value is %d\n", len);
				if (len != pmt_len)
				{
					printf("send error, client fd is %d, return value is %d, errno=%d\n", client_fd[i], len, errno);
					perror("");
					close(client_fd[i]);
					client_fd[i] = -1;
				}
			}
		}
		pthread_mutex_unlock(&connections_table_lock);
	}

	pthread_mutex_unlock(&pmt_distribute_lock);

	pthread_exit(NULL);
}

void init_daemon(void)
{
	int pid;
	int fd, fdtablesize;

	/* 忽略终端 I/O信号，STOP信号 */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/*忽略网络断开信号*/
	signal(SIGPIPE, SIG_IGN);

	if(pid = fork())
		exit(0);		//是父进程，结束父进程
	else if(pid < 0)
		exit(1);		//fork失败，退出

	//是第一子进程，后台继续执行
	setsid();		//第一子进程成为新的会话组长和进程组长并与控制终端分离

	if(pid = fork())
		exit(0);		//是第一子进程，结束第一子进程
	else if(pid < 0)
		exit(1);		//fork失败，退出
	//是第二子进程，继续
	//第二子进程不再是会话组长

	for (fd = 0, fdtablesize = getdtablesize(); fd < fdtablesize; fd++)
		close (fd);	//关闭打开的文件描述符

	chdir("/tmp");	//改变工作目录到/tmp
	umask(0);		//重设文件创建掩模
	return;
}

static void on_termination(int signo)
{
	if (pmt_socket_backuped != 0)
	{
		unlink(PMT_SERVER_SOCKET);
		link(PMT_SERVER_SOCKET_BACKUP, PMT_SERVER_SOCKET);
		unlink(PMT_SERVER_SOCKET_BACKUP);
	}
	exit(0);
}

void print_copyright()
{
	printf("\n");
	printf("/============================================================\\\n");
	printf("| PMT Table Transformer                                      |\n");
	printf("| Version 1.0 Built 01/17/2008                               |\n");
	printf("| Copyright (c) 2008 DLSoftware                              |\n");
	printf("|                                                            |\n");
	printf("| This program is another implementation of the method which |\n");
	printf("| findout by ptjiang to let CCcam to process multi ecm.      |\n");
	printf("|                                                            |\n");
	printf("| Creator: Dailin                                            |\n");
	printf("| Contact: linking_dai@163.com                               |\n");
	printf("| Forum:   http://www.xltvrobbs.net                          |\n");
	printf("\\===========================================================/\n");
	printf("\n\n");}

int main(int argc, char **argv)
{
	print_copyright();

	if (argc < 2 || strcmp(argv[1], "-d") != 0)
		init_daemon();

	int pmtfd = -1;
	struct sockaddr_un saddr;
	unsigned char pmtbuf[1024];
	int pmtlen;
	int new_pmt_server_threads_created = 0;

	signal(SIGINT, on_termination);
	signal(SIGTERM, on_termination);

	pthread_mutex_init(&pmt_distribute_lock, NULL);
	pthread_cond_init(&new_pmt_arrived, NULL);

	memset(client_fd, -1, sizeof(client_fd));

	while (1)
	{
		printf("Trying connect to PMT listen socket...\n");

		/* socket init */
		saddr.sun_family = AF_UNIX;
		if (pmt_socket_backuped != 0)
			strncpy(saddr.sun_path, PMT_SERVER_SOCKET_BACKUP, 107);
		else
			strncpy(saddr.sun_path, PMT_SERVER_SOCKET, 107);
		saddr.sun_path[107] = '\0';
		if (pmtfd == -1)
		{
			if ((pmtfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
			{
				printf("socket error (errno=%d)\n", errno);
				perror("");
				return 1;
			}
		}
		if (connect(pmtfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
			printf("socket connect error (errno=%d)\n", errno);
			perror("");
			usleep(200);
			continue;
		}

		printf("Connected.\n");

		if (pmt_socket_backuped == 0)
		{
			unlink(PMT_SERVER_SOCKET_BACKUP);
			link(PMT_SERVER_SOCKET, PMT_SERVER_SOCKET_BACKUP);
			pmt_socket_backuped = 1;
		}

		if (new_pmt_server_threads_created == 0)
		{
			pthread_t new_pmt_server_thread;
			pthread_attr_t new_pmt_server_thread_attr;
			pthread_attr_init (&new_pmt_server_thread_attr);
			pthread_attr_setdetachstate (&new_pmt_server_thread_attr, PTHREAD_CREATE_DETACHED);
			if(pthread_create(&new_pmt_server_thread, &new_pmt_server_thread_attr, new_pmt_server_proc, NULL) < 0)
			{
				printf("failed to create new pmt server thread (errno=%d).\n", errno);
				perror("");
				return 5;
			}

			pthread_t pmt_distribute_thread;
			pthread_attr_t pmt_distribute_thread_attr;
			pthread_attr_init (&pmt_distribute_thread_attr);
			pthread_attr_setdetachstate (&pmt_distribute_thread_attr, PTHREAD_CREATE_DETACHED);
			if(pthread_create(&pmt_distribute_thread, &pmt_distribute_thread_attr, pmt_distribute_proc, NULL) < 0)
			{
				printf("failed to create pmt distribute thread (errno=%d).\n", errno);
				perror("");
				return 5;
			}

			new_pmt_server_threads_created = 1;
		}

		printf("Waiting for PMTs...\n");

		while (1)
		{
			pmtlen = recv(pmtfd, pmtbuf, sizeof(pmtbuf), 0);
			if (pmtlen < 0)
			{
				close(pmtfd);
				pmtfd = -1;
				printf("recv error (errno=%d)\n", errno);
				perror("");
				break;
			}
			else if (pmtlen == 0)
			{
				close(pmtfd);
				pmtfd = -1;
				printf("server connection closed.");
				break;
			}

			utils_dump("Original PMT: ", pmtbuf, pmtlen);

			pthread_mutex_lock(&pmt_distribute_lock);

			pmt_len = convert_pmt(pmtbuf, pmtlen, pmt_buffer);

			pthread_cond_signal(&new_pmt_arrived);

			pthread_mutex_unlock(&pmt_distribute_lock);

			utils_dump("New PMT: ", pmt_buffer, pmt_len);
		}
	}

	return 0;
}

