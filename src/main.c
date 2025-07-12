#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "ping.h"

typedef struct {
    const char* target_ip;
    struct ping_stats* stats;
} ping_thread_args_t;

void* ping_target_thread(void* arg) {
    ping_thread_args_t* args = (ping_thread_args_t*)arg;
    const char* ip = args->target_ip;
    struct ping_stats* stats = args->stats;

    int sockfd = init_ping_socket();
    if (sockfd < 0) {
        pthread_exit(NULL);
    }

    for (int seq = 1; seq <= 3; seq++) {
        double rtt_ms = 0.0;
        int received = 0;

        if (send_ping(ip, seq) == 0) {
            if (receive_ping_reply(sockfd, &rtt_ms) == 0) {
                received = 1;
            } else {
                printf("No reply for seq=%d\n", seq);
            }
        }
        update_stats(stats, rtt_ms, received);
        print_live_status(stats);
        sleep(1);
    }

    print_stats_summary(stats);
    pthread_exit(NULL);
}

int main() {
    printf("Netpulse - Network Monitoring Tool\n");
    printf("========================\n");

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);


    const char* targets[] = {
        "8.8.8.8",
        "1.1.1.1",
        "208.67.222.222",
        "192.0.2.1",
    };

    int num_targets = sizeof(targets) / sizeof(targets[0]);
    pthread_t threads[num_targets];
    ping_thread_args_t args[num_targets];
    struct ping_stats stats[num_targets];

    for (int i = 0; i < num_targets; i++) {
        init_stats(&stats[i], targets[i]);
        args[i].target_ip = targets[i];
        args[i].stats = &stats[i];
        pthread_create(&threads[i], NULL, ping_target_thread, &args[i]);
    }

    for (int i = 0; i < num_targets; i++) {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&end_time, NULL);
    double elapsed_sec = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_usec - start_time.tv_usec) / 1000000.0;

    printf("\nTotal execution time: %.3f seconds\n", elapsed_sec);


    printf("\n=== OVERALL SUMMARY ===\n");
    for (int i = 0; i < num_targets; i++) {
        printf("%s: %.1f%% packet loss\n", targets[i], calculate_packet_loss(&stats[i]));
        printf("%s: %.1fms jitter\n", targets[i], calculate_jitter(&stats[i]));
    }

    return 0;
}