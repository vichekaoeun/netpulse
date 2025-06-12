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

struct ping_stats {
    char target[256];
    int packets_sent;
    int packets_received;
    double min_rtt;
    double max_rtt;
    double total_rtt;
    double last_rtt;
};

int init_ping_socket();
int send_ping(const char *target_ip, int sequence);
int receive_ping_reply(int sockfd, double *rtt_ms);
uint16_t calculate_checksum(void *data, int len);
void cleanup_ping_socket();

void init_stats(struct ping_stats *stats, const char *target);
void update_stats(struct ping_stats *stats, double rtt_ms, int received);
double calculate_packet_loss(struct ping_stats *stats);
double calculate_average_rtt(struct ping_stats *stats);
void print_stats_summary(struct ping_stats *stats);
void print_live_status(struct ping_stats *stats);

#endif