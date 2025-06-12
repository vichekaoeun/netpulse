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

uint16_t calculate_checksum(void *data, int len)
{
    uint16_t *ptr = (uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1)
    {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1)
    {
        sum += *(uint8_t *)ptr;
    }

    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum; // return the complement
}

int init_ping_socket()
{
    if (ping_sockfd != -1)
    {
        return ping_sockfd;
    }

    ping_sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); // IPROTO_ICMP tells endpoint to send ICMP packets
    if (ping_sockfd == -1)
    {
        perror("Socket creation failed");
        return -1;
    }

    printf("ICMP socket created successfully.\n");
    return ping_sockfd;
}

int send_ping(const char *target_ip, int sequence)
{
    if (ping_sockfd == -1)
    {
        printf("Socket not initialized.\n");
        return -1;
    }

    struct sockaddr_in target;
    struct icmp_packet packet;
    struct timeval tv;

    memset(&target, 0, sizeof(target)); // clear
    target.sin_family = AF_INET;

    if (inet_pton(AF_INET, target_ip, &target.sin_addr) <= 0) // check if target IP is valid
    {
        printf("Invalid IP address: %s\n", target_ip);
        return -1;
    }

    memset(&packet, 0, sizeof(packet));

    packet.type = ICMP_ECHO; // echo request
    packet.code = 0;
    packet.id = getpid();
    packet.sequence = sequence;
    packet.checksum = 0;

    gettimeofday(&tv, NULL);
    memcpy(packet.code, &tv, sizeof(tv)); // copy current time to packet data

    packet.checksum = calculate_checksum(&packet, sizeof(packet));

    if (sendto(ping_sockfd, &packet, sizeof(packet), 0,
               (struct sockaddr *)&target, sizeof(target)) < 0)
    {
        perror("Failed to send ping");
        return -1;
    }

    printf("Ping sent to %s (seq=%d)\n", target_ip, sequence);
    return 0;
}

int receive_ping_reply(int sockfd, double *rtt_ms)
{
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
            printf("Ping timeout\n");
            return -1;
        }
        perror("Failed to receive ping reply");
        return -1;
    }

    gettimeofday(&recv_time, NULL);

    //Skip IP header to get to ICMP

    struct ip* ip_header = (struct ip*)buffer;
    int ip_header_len = ip_header->ip_hl * 4;

    if (bytes_received < ip_header_len + 8) { //packet might not be big enough
        printf("Received packet too short\n");
        return -1;
    }

    struct icmp* icmp_header = (struct icmp*)(buffer + ip_header_len); //grab ICMP header

    if (icmp_header->icmp_type == ICMP_ECHOREPLY && //check if packet is REPLY
        icmp_header->icmp_id == getpid()) {
        
        //get send time
        memcpy(&send_time, icmp_header->icmp_data, sizeof(send_time));
        
        // calculate RTT in milliseconds
        *rtt_ms = (recv_time.tv_sec - send_time.tv_sec) * 1000.0;
        *rtt_ms += (recv_time.tv_usec - send_time.tv_usec) / 1000.0;
        
        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender.sin_addr, sender_ip, INET_ADDRSTRLEN);
        
        printf("Reply from %s: seq=%d time=%.2f ms\n", 
               sender_ip, icmp_header->icmp_seq, *rtt_ms);
        
        return 0;
    }
    
    return -1; // Not our packet
}

void cleanup_ping_socket() { //clean
    if (ping_sockfd != -1) {
        close(ping_sockfd);
        ping_sockfd = -1;
    }
}