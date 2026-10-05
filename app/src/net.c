#include "net.h"

#include <zephyr/net/socket.h>

static int sock = -1;
static struct sockaddr peer;
static socklen_t peer_len;

int net_open(void)
{
	struct sockaddr_in a = {
		.sin_family = AF_INET,
		.sin_port = htons(NODE_UDP_PORT),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};

	sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		return -1;
	}
	return zsock_bind(sock, (struct sockaddr *)&a, sizeof(a));
}

int net_recv(uint8_t *buf, size_t cap, int timeout_ms)
{
	struct zsock_pollfd p = {.fd = sock, .events = ZSOCK_POLLIN};

	if (zsock_poll(&p, 1, timeout_ms) <= 0) {
		return -1;
	}
	struct sockaddr from;
	socklen_t fl = sizeof(from);
	int n = zsock_recvfrom(sock, buf, cap, 0, &from, &fl);

	if (n >= 0) {
		peer = from;
		peer_len = fl;
	}
	return n;
}

void net_reply(const uint8_t *buf, size_t len)
{
	if (peer_len != 0) {
		zsock_sendto(sock, buf, len, 0, &peer, peer_len);
	}
}
