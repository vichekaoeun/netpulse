#ifndef PING_H
#define PING_H

#include <stdint.h>

struct icmp_packet
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
    double timestamp;
};

int send_ping(const char *target_ip, int sequence);
int receive_ping_reply(int sockfd, double *rtt_ms);
uint16_t calculate_checksum(void *data, int len);

#endif