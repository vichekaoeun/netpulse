#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include "ping.h"

static int ping_sockfd = -1;
int log_to_tui = 1;

uint16_t calculate_checksum(void *data, int len) {
    uint16_t* ptr = (uint16_t*)data;
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(uint8_t*)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

int init_ping_socket() {
    if (ping_sockfd != -1) {
        return ping_sockfd; 
    }
    
    ping_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (ping_sockfd == -1) {
        if (!log_to_tui) {
            perror("Failed to create raw socket");
            printf("Note: Raw sockets require root privileges. Try: sudo ./netpulse\n");
        }
        return -1;
    }
    
    if (!log_to_tui) {
        printf("ICMP socket created successfully\n");
    }
    return ping_sockfd;
}

int send_ping(const char* target_ip, int sequence) {
    if (ping_sockfd == -1) {
        if (!log_to_tui) {
            printf("Socket not initialized. Call init_ping_socket() first.\n");
        }
        return -1;
    }
    
    struct sockaddr_in target;
    struct icmp_packet packet;  
    struct timeval tv;
    
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, target_ip, &target.sin_addr) <= 0) {
        if (!log_to_tui) {
            printf("Invalid IP address: %s\n", target_ip);
        }
        return -1;
    }
    
    memset(&packet, 0, sizeof(packet));
    packet.type = 8;           
    packet.code = 0;              
    packet.id = getpid();        
    packet.sequence = sequence;   
    packet.checksum = 0;         
    gettimeofday(&tv, NULL);
    packet.timestamp = tv.tv_sec + tv.tv_usec / 1000000.0;
    
    packet.checksum = calculate_checksum(&packet, sizeof(packet));
    
    if (sendto(ping_sockfd, &packet, sizeof(packet), 0,
               (struct sockaddr*)&target, sizeof(target)) < 0) {
        if (!log_to_tui) {
            perror("Failed to send ping");
        }
        return -1;
    }
    
    return 0;
}

int receive_ping_reply(int sockfd, double* rtt_ms) {
    char buffer[1024];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    struct timeval recv_time, send_time;
    ssize_t bytes_received;
    
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&sender, &sender_len);
    
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (!log_to_tui) {
                printf("Ping timeout\n");
            }
            return -1;
        }
        if (!log_to_tui) {
            perror("Failed to receive ping reply");
        }
        return -1;
    }
    
    gettimeofday(&recv_time, NULL);
    
    struct ip* ip_header = (struct ip*)buffer;
    int ip_header_len = ip_header->ip_hl * 4;
    
    if (bytes_received < ip_header_len + 8) {
        if (!log_to_tui) {
            printf("Received packet too short\n");
        }
        return -1;
    }
    
    struct icmp* icmp_header = (struct icmp*)(buffer + ip_header_len);
    
    if (icmp_header->icmp_type == 0 && 
        icmp_header->icmp_id == getpid()) {
        
        double send_timestamp;
        memcpy(&send_timestamp, icmp_header->icmp_data, sizeof(send_timestamp));
        
        double recv_timestamp = recv_time.tv_sec + recv_time.tv_usec / 1000000.0;
        *rtt_ms = (recv_timestamp - send_timestamp) * 1000.0;
        
        return 0;
    }
    
    return -1;
}

void cleanup_ping_socket() {
    if (ping_sockfd != -1) {
        close(ping_sockfd);
        ping_sockfd = -1;
    }
}