#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ncurses.h>
#include "ping.h"

#define INTERVAL 5
#define REFRESH_INTERVAL 1
#define COLOR_EXCELLENT 1
#define COLOR_GOOD 2
#define COLOR_FAIR 3
#define COLOR_POOR 4

typedef struct {
    const char* target_ip;
    struct ping_stats* stats;
} ping_thread_args_t;

void init_curses() {
    initscr();
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    init_pair(COLOR_EXCELLENT, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_GOOD, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_FAIR, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_POOR, COLOR_RED, COLOR_BLACK);
}

void draw_header(double elapsed_sec) {
    attron(A_BOLD | A_UNDERLINE);
    mvprintw(0, 0, "Netpulse - Network Monitoring Tool");
    mvprintw(1, 0, "Running for %.1f seconds", elapsed_sec);
    attroff(A_BOLD | A_UNDERLINE);
}

void draw_table_header(int start_row, int cols) {
    attron(A_BOLD);
    mvprintw(start_row, 0, "%-15s %8s %8s %8s %10s %10s %8s %s",
             "Target IP", "Sent", "Recv", "Loss%", "Last RTT", "Jitter", "Activity", "Quality");
    attroff(A_BOLD);
    mvhline(start_row + 1, 0, ACS_HLINE, cols);
}

void draw_stats(int row, struct ping_stats* stats) {
    double packet_loss = calculate_packet_loss(stats);
    double jitter = calculate_jitter(stats);
    const char* quality = assess_network_quality(stats);
    int color_pair;

    if (strcmp(quality, "Excellent (Very low jitter)") == 0) {
        color_pair = COLOR_EXCELLENT;
    } else if (strcmp(quality, "Good (Low jitter)") == 0) {
        color_pair = COLOR_GOOD;
    } else if (strcmp(quality, "Fair (Moderate jitter)") == 0) {
        color_pair = COLOR_FAIR;
    } else {
        color_pair = COLOR_POOR;
    }

    const char* activity_char = " |/-\\";
    char activity = activity_char[stats->packet_activity % 4];

    attron(COLOR_PAIR(color_pair));
    mvprintw(row, 0, "%-15s %8d %8d %8.1f %10.2f %10.2f %8c %s",
             stats->target, stats->packets_sent, stats->packets_received,
             packet_loss, stats->last_rtt, jitter, activity, quality);
    attroff(COLOR_PAIR(color_pair));
}

void draw_footer(int row, int cols) {
    mvhline(row - 2, 0, ACS_HLINE, cols);
    mvprintw(row - 1, 0, "Press 'q' to quit");
}

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

    FILE* pid_file = fopen("/tmp/netpulse.pid", "w");
    if (pid_file) {
        fprintf(pid_file, "%d\n", getpid());
        fclose(pid_file);
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
        sleep(INTERVAL);
    }

    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--daemon") == 0) {
        daemonize();
        const char* targets[] = {"8.8.8.8", "1.1.1.1", "208.67.222.222", "192.0.2.1"};
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

    init_curses();

    const char* targets[] = {"8.8.8.8", "1.1.1.1", "208.67.222.222", "192.0.2.1"};
    int num_targets = sizeof(targets) / sizeof(targets[0]);
    pthread_t threads[num_targets];
    ping_thread_args_t args[num_targets];
    struct ping_stats stats[num_targets];
    struct timeval start_time;

    gettimeofday(&start_time, NULL);

    for (int i = 0; i < num_targets; i++) {
        init_stats(&stats[i], targets[i]);
        args[i].target_ip = targets[i];
        args[i].stats = &stats[i];
        pthread_create(&threads[i], NULL, ping_target_thread, &args[i]);
    }

    while (1) {
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        clear();

        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        double elapsed_sec = (current_time.tv_sec - start_time.tv_sec) +
                             (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

        draw_header(elapsed_sec);
        draw_table_header(3, max_x);
        for (int i = 0; i < num_targets; i++) {
            stats[i].packet_activity = (stats[i].packet_activity + 1) % 4;
            draw_stats(4 + i, &stats[i]);
        }
        draw_footer(max_y, max_x);

        refresh();
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            break;
        }
        sleep(REFRESH_INTERVAL);
    }

    for (int i = 0; i < num_targets; i++) {
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
    }
    cleanup_ping_socket();
    endwin();

    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double elapsed_sec = (end_time.tv_sec - start_time.tv_sec) +
                         (end_time.tv_usec - start_time.tv_usec) / 1000000.0;

    printf("\nPing test complete.\n");
    printf("\n=== OVERALL SUMMARY ===\n");
    for (int i = 0; i < num_targets; i++) {
        print_stats_summary(&stats[i]);
    }
    printf("\nTotal execution time: %.3f seconds\n", elapsed_sec);

    return 0;
}