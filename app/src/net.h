#ifndef NET_H
#define NET_H

#include <stddef.h>
#include <stdint.h>

#define NODE_UDP_PORT 5555

int net_open(void);
/* Blocking receive with timeout; returns length or a negative value. */
int net_recv(uint8_t *buf, size_t cap, int timeout_ms);
/* Send to the peer of the last received datagram (no-op before the first one). */
void net_reply(const uint8_t *buf, size_t len);

#endif
