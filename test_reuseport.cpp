#include <iostream>
// std
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#undef __STDC_FORMAT_MACROS
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>

#include <map>
#include <string>

// linux
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/resource.h>

// net
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/if_ether.h>
#include <linux/netfilter_ipv4.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <linux/icmp.h>
#include <stdio.h>


static volatile int worker_num;
#define OPT_SIZE sizeof(int)

#define UDP_MSG_MAX_LEN 102400
#define NIPQUAD_FMT "%d.%d.%d.%d"
#define NIPQUAD_ADDR(addr) \
        &((unsigned char*)&(addr))[0], \
        &((unsigned char*)&(addr))[1], \
        &((unsigned char*)&(addr))[2], \
        &((unsigned char*)&(addr))[3]
#define NIPQUAD(addr) \
        ((unsigned char*)&(addr))[0], \
        ((unsigned char*)&(addr))[1], \
        ((unsigned char*)&(addr))[2], \
        ((unsigned char*)&(addr))[3]

using namespace std;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
map<uint16_t, int> sock_map; // port, worker_id

void *recv_thread(void *arg) {
	int * i = (int *)(arg);
	printf("worker %d start\n", *i);
	int OPT_ON = 1;
	char cntrlbuf[64];
	struct iovec iov;
	struct sockaddr_in src_addr, dst_addr;
	struct msghdr msg;
	char buff[UDP_MSG_MAX_LEN];
	struct sockaddr_in sin;	
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr("0.0.0.0");
	sin.sin_port = htons(7777);
	char ip_buf[50];
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		cout <<  "socket fail: " << strerror(errno) << endl;
		goto error;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &OPT_ON, OPT_SIZE) < 0) {
		cout << "setsockopt SO_REUSEADDR fail: "  << strerror(errno) << endl;
		goto error;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &OPT_ON, OPT_SIZE) < 0) {
		cout << "set SO_REUSEPORT fail: " << strerror(errno) << endl;
		goto error;
	}
	bind(fd, (struct sockaddr*)&sin, sizeof(sin));

	while (1) {
		msg.msg_name = &src_addr;
		msg.msg_namelen = sizeof(src_addr);
		msg.msg_control = cntrlbuf;
		msg.msg_controllen = sizeof(cntrlbuf);
		iov.iov_base = buff;
		iov.iov_len = UDP_MSG_MAX_LEN;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		int length = recvmsg(fd, &msg, 0);
		if (length <= 0) {
			cout << "recvmsg fail" << strerror(errno) << endl;
			continue;
		}
		sendto(fd, buff, length, 0, (struct sockaddr*)&src_addr, sizeof(src_addr));
		uint16_t port = htons(src_addr.sin_port);
		pthread_mutex_lock(&lock);
		if (sock_map.find(port) != sock_map.end()) {
			if (sock_map[port] != *i) {
				printf("error: port:%d not match old:%d new:%d\n", port, sock_map[port], *i);
			}
		} else {
			printf("accept new sock port:%d in worker:%d \n", htons(src_addr.sin_port), *i);
			sock_map[htons(src_addr.sin_port)] = *i;
		}
		pthread_mutex_unlock(&lock);
	}
	return NULL;
error:
	cout << "error" <<endl;
}

int main(int argc, char **argv) {
	int count = 4;
	if (argc >= 2) {
		count = atoi(argv[1]);
	}
	worker_num = -1;
	for (int i = 1; i <= count; i++) {
		pthread_t thread_id;
		int *t = new int;
		*t = i;
		pthread_create(&thread_id, NULL, recv_thread, t);
	}

	while(1) {
		sleep(1);
	}
}

