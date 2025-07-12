#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ping.h"

#define INTERVAL 5

typedef struct {
    const char* target_ip;
    struct ping_stats* stats;
} ping_thread_args_t;

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    setsid();
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_RDWR);

    int fd = open("/tmp/netpulse.log", O_RDWR | O_CREAT | O_APPEND, 0600);
    if (fd != -1) {
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

void* ping_target_thread(void* arg) {
    ping_thread_args_t* args = (ping_thread_args_t*)arg;
    const char* ip = args->target_ip;
    struct ping_stats* stats = args->stats;

    int sockfd = init_ping_socket();
    if (sockfd < 0) {
        pthread_exit(NULL);
    }

    while (1) {
        double rtt_ms = 0.0;
        int received = 0;

        if (send_ping(ip, stats->packets_sent + 1) == 0) {
            if (receive_ping_reply(sockfd, &rtt_ms) == 0) {
                received = 1;
            }
        }

        update_stats(stats, rtt_ms, received);
        print_live_status(stats);

        sleep(INTERVAL);
    }

    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--daemon") == 0) {
        daemonize();
    }

    printf("Netpulse - Network Monitoring Tool\n");
    printf("========================\n");

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

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

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

    printf("\nPing test complete.\n");
    printf("\n=== OVERALL SUMMARY ===\n");
    for (int i = 0; i < num_targets; i++) {
        printf("%s: %.1f%% packet loss\n", targets[i], calculate_packet_loss(&stats[i]));
        printf("%s: %.1fms jitter\n", targets[i], calculate_jitter(&stats[i]));
    }

    printf("\nTotal execution time: %.3f seconds\n", elapsed_sec);

    return 0;
}
