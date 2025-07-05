#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ping.h"

void init_stats(struct ping_stats *stats, const char *target) {
    strncpy(stats->target, target, sizeof(stats->target) - 1);
    stats->target[sizeof(stats->target) - 1] = '\0';
    
    stats->packets_sent = 0;
    stats->packets_received = 0;
    stats->min_rtt = INFINITY;
    stats->max_rtt = 0.0;
    stats->total_rtt = 0.0;
    stats->last_rtt = 0.0;
    stats->previous_rtt = 0.0;
    stats->total_jitter = 0.0;
    stats->jitter_count = 0;
}

void update_stats(struct ping_stats *stats, double rtt_ms, int received) {
    stats->packets_sent++;
    
    if (received) {
        stats->packets_received++;

        if (stats->packets_received > 1) {
            double jitter = fabs(rtt_ms - stats->last_rtt);
            stats->total_jitter += jitter;
            stats->jitter_count++; 
        }

        stats->last_rtt = rtt_ms;
        stats->total_rtt += rtt_ms;
        
        if (rtt_ms < stats->min_rtt) {
            stats->min_rtt = rtt_ms;
        }
        if (rtt_ms > stats->max_rtt) {
            stats->max_rtt = rtt_ms;
        }
    }
}

double calculate_packet_loss(struct ping_stats *stats) {
    if (stats->packets_sent == 0) {
        return 0.0;
    }
    return ((double)(stats->packets_sent - stats->packets_received) / stats->packets_sent) * 100.0;
}

double calculate_average_rtt(struct ping_stats *stats) {
    if (stats->packets_received == 0) {
        return 0.0;
    }
    return stats->total_rtt / stats->packets_received;
}

double calculate_jitter(struct ping_stats *stats) {
    if (stats->jitter_count == 0) {
        return 0.0;
    }
    return stats->total_jitter / stats->jitter_count; 
}

void print_stats_summary(struct ping_stats *stats) {
    printf("\n--- %s ping statistics ---\n", stats->target);
    printf("%d packets transmitted, %d received, %.1f%% packet loss\n",
           stats->packets_sent, stats->packets_received, calculate_packet_loss(stats));
    
    if (stats->packets_received > 0) {
        printf("round-trip min/avg/max = %.2f/%.2f/%.2f ms\n",
               stats->min_rtt, calculate_average_rtt(stats), stats->max_rtt);
        
        if (stats->jitter_count > 0) {
            printf("jitter = %.2f ms (avg variation between consecutive pings)\n",
                   calculate_jitter(stats));
        }
    }
}

void print_live_status(struct ping_stats *stats) {
    printf("[%s] Sent: %d, Received: %d, Loss: %.1f%%, Last: %.2fms",
           stats->target, stats->packets_sent, stats->packets_received,
           calculate_packet_loss(stats), stats->last_rtt);
    
    if (stats->jitter_count > 0) {
        printf(", Jitter: %.2fms", calculate_jitter(stats));
    }
    printf("\n");
}

const char* assess_network_quality(struct ping_stats *stats) {
    double jitter = calculate_jitter(stats);
    double packet_loss = calculate_packet_loss(stats);
    
    if (packet_loss > 5.0) {
        return "Poor (High packet loss)";
    } else if (jitter > 50.0) {
        return "Poor (High jitter)";
    } else if (jitter > 20.0) {
        return "Fair (Moderate jitter)";
    } else if (jitter > 10.0) {
        return "Good (Low jitter)";
    } else {
        return "Excellent (Very low jitter)";
    }
}