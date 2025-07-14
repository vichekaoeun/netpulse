#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ping.h"

#define METRICS_FILE "/tmp/netpulse.prom"

void write_prometheus_metrics(struct ping_stats* stats_array, int count) {
    FILE* f = fopen(METRICS_FILE, "w");
    if (!f) return;

    fprintf(f, "# HELP netpulse_rtt_ms Round-trip time in milliseconds\n");
    fprintf(f, "# TYPE netpulse_rtt_ms gauge\n");
    fprintf(f, "# HELP netpulse_packet_loss Packet loss percentage\n");
    fprintf(f, "# TYPE netpulse_packet_loss gauge\n");
    fprintf(f, "# HELP netpulse_jitter_ms Jitter in milliseconds\n");
    fprintf(f, "# TYPE netpulse_jitter_ms gauge\n");

    for (int i = 0; i < count; ++i) {
        fprintf(f, "netpulse_rtt_ms{target=\"%s\"} %.2f\n",
                stats_array[i].target, calculate_average_rtt(&stats_array[i]));
        fprintf(f, "netpulse_packet_loss{target=\"%s\"} %.2f\n",
                stats_array[i].target, calculate_packet_loss(&stats_array[i]));
        fprintf(f, "netpulse_jitter_ms{target=\"%s\"} %.2f\n",
                stats_array[i].target, calculate_jitter(&stats_array[i]));
    }

    fclose(f);
}