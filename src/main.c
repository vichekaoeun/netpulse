#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ping.h"

int main()
{
    printf("Netpulse -Network Monitoring Tool\n");
    printf("=========================\n");

    if (init_ping_socket() < 0) {
        printf("Failed to initialize ping socket. Exiting.\n");
        return 1;
    }

    const char* targets[] = {
        "8.8.8.8",      // Google DNS
        "1.1.1.1",      // Cloudflare DNS
    };

    int num_targets = sizeof(targets) / sizeof(targets[0]);

    printf("\nTesting ICMP ping to multiple targets:\n");
    printf("--------------------------------------\n");

    for (int i = 0; i < num_targets; i++) {
        printf("\nPinging %s:\n", targets[i]);
        
        for (int seq = 1; seq <= 3; seq++) {
            double rtt_ms;
            
            //send ping
            if (send_ping(targets[i], seq) == 0) {
                if (receive_ping_reply(init_ping_socket(), &rtt_ms) == 0) {
                } else {
                    printf("No reply for seq=%d\n", seq);
                }
            }
            
            sleep(1); //wait 1 sec between pings
        }
    }
    
    cleanup_ping_socket();
    printf("\nPing test complete.\n");
    return 0;
}