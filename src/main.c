#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

int test_tcp_connection(const char *ip, int port)
{
    int sock;
    struct sockaddr_in server;
    struct timeval start, end;
    double connect_time;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        perror("Socket creation failed");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server.sin_addr) <= 0)
    { // if conversion from text to binary form fails because of an invalid address
        perror("Invalid address");
        close(sock);
        return -1;
    }

    gettimeofday(&start, NULL); // get the current time before connecting

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    { // if connection fails
        printf("Connection failed to: %s:%d %s\n", ip, port, strerror(errno));
        close(sock);
        return -1;
    }

    gettimeofday(&end, NULL); // get the current time after connection ends

    connect_time = (end.tv_sec - start.tv_sec) * 1000.0;
    connect_time += (end.tv_usec - start.tv_usec) / 1000.0;

    printf("Connected to %s:%d in %.2f ms\n", ip, port, connect_time);

    close(sock);
    return 0;
}

int main()
{
    printf("Basic TCP Connection Test\n");
    printf("=========================\n");

    // Test various servers
    test_tcp_connection("8.8.8.8", 53);     // Google DNS
    test_tcp_connection("1.1.1.1", 53);     // Cloudflare DNS
    test_tcp_connection("8.8.8.8", 443);    // Google HTTPS (might work)
    test_tcp_connection("1.1.1.1", 443);    // Cloudflare HTTPS (should work)
    test_tcp_connection("192.168.1.1", 22); // Local router SSH (likely fails)

    return 0;
}