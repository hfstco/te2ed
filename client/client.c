#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int main(int argc, char **argv) {
    int client_fd;
    struct sockaddr_in server_addr;
    struct timespec current_time;
    uint64_t client_send_timestamp, server_recv_timestamp, server_send_timestamp, client_recv_timestamp;
    FILE* log_file;
    int flag = 1;

    /* Read CMD arguments. */
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <ip_addr> <port> <iterations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *ip_addr = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Port must be between 1 and 65535\n");
        exit(EXIT_FAILURE);
    }
    int iterations = atoi(argv[3]);

    // Open log file in append mode
    log_file = fopen(argv[4], "w");
    if (log_file == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
    }
    // Write csv header
    fprintf(log_file, "CLIENT_RECV_TIME,SERVER_SENT_TIME,SERVER_RECV_TIME,CLIENT_SENT_TIME\n");

    // Create socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set TCP_NODELAY on the client socket
    if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        perror("setsockopt TCP_NODELAY failed");
        close(client_fd);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_addr, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address\n");
        exit(EXIT_FAILURE);
    }
    server_addr.sin_port = htons(port);

    fprintf(stdout, "Client ready.\n");
    fprintf(stdout, "[CLIENT_RECV_TIME][SERVER_SENT_TIME][SERVER_RECV_TIME][CLIENT_SENT_TIME]\n");

    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < iterations; i++) {
        // Get current time for sending and logging
        clock_gettime(CLOCK_REALTIME, &current_time);
        client_send_timestamp = (current_time.tv_sec * 1000000ull) + current_time.tv_nsec / 1000ull;

        // Send current timestamp
        write(client_fd, &client_send_timestamp, sizeof(uint64_t));
        fprintf(stdout, "<- [][][][%" PRIu64 "]\n", client_send_timestamp);

        // Read timestamps from response
        ssize_t bytes_read = read(client_fd, &client_send_timestamp, sizeof(uint64_t));
        if (bytes_read <= 0) {
            exit(EXIT_FAILURE);
        }
        bytes_read = read(client_fd, &server_recv_timestamp, sizeof(uint64_t));
        if (bytes_read <= 0) {
            exit(EXIT_FAILURE);
        }
        bytes_read = read(client_fd, &server_send_timestamp, sizeof(uint64_t));
        if (bytes_read <= 0) {
            exit(EXIT_FAILURE);
        }

        // Get current time for receive logging
        clock_gettime(CLOCK_REALTIME, &current_time);
        client_recv_timestamp = (current_time.tv_sec * 1000000ull) + current_time.tv_nsec / 1000ull;

        fprintf(stdout, "-> [%" PRIu64 "][%" PRIu64 "][%" PRIu64 "][%" PRIu64 "]\n", client_recv_timestamp, server_send_timestamp, server_recv_timestamp, client_send_timestamp);

        // Log to file
        fprintf(log_file, "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                client_recv_timestamp, server_send_timestamp, server_recv_timestamp, client_send_timestamp);
    }

    close(client_fd);
    return 0;
}
