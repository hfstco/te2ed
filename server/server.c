//
// Created by Hofstätter, Matthias on 01.07.25.
//


#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/errno.h>

int main(int argc, char **argv) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct timespec current_time;
    uint64_t client_send_timestamp, server_recv_timestamp, server_send_timestamp;
    int flag = 1;

    /* Read CMD arguments. */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Port must be between 1 and 65535\n");
        exit(EXIT_FAILURE);
    }

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR to allow quick restart
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }


    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Server ready.\n");
    fprintf(stdout, "[CLIENT_RECV_TIME][SERVER_SENT_TIME][SERVER_RECV_TIME][CLIENT_SENT_TIME]\n");

    while (1) {
        // Accept client connection
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        // Set TCP_NODELAY on the client socket
        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            perror("setsockopt TCP_NODELAY failed");
            close(client_fd);
            continue;
        }


        fprintf(stdout, "\nClient connected from %s:%d\n",
                inet_ntoa(client_addr.sin_addr),
                ntohs(client_addr.sin_port));

        while (1) {
            /* Receive send timestamp from client. */
            ssize_t bytes_read = read(client_fd, &client_send_timestamp, sizeof(uint64_t));
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    fprintf(stdout, "Client disconnected.\n");
                    break;
                }
                perror("Read failed.\n");
                break;
            }

            /* Get recv time. */
            clock_gettime(CLOCK_REALTIME, &current_time);
            server_recv_timestamp = (current_time.tv_sec * 1000000ull) + current_time.tv_nsec / 1000ull;
            fprintf(stdout, "-> [][][%" PRIu64 "][%" PRIu64 "]\n", server_recv_timestamp, client_send_timestamp);

            /* Server processing here. */

            /* Get send time. */
            clock_gettime(CLOCK_REALTIME, &current_time);
            server_send_timestamp = (current_time.tv_sec * 1000000ull) + current_time.tv_nsec / 1000ull;
            /* Prepare buffer. */
            char* buffer = malloc(3 * sizeof(uint64_t));
            memcpy(buffer, &client_send_timestamp, sizeof(uint64_t));
            memcpy(buffer + sizeof(uint64_t), &server_recv_timestamp, sizeof(uint64_t));
            memcpy(buffer + (2 * sizeof(uint64_t)), &server_send_timestamp, sizeof(uint64_t));
            /* Send buffer. */
            ssize_t bytes_written = write(client_fd, buffer, 3 * sizeof(uint64_t));
            if (bytes_written <= 0) {
                perror("Write failed.\n");
                break;
            }
            free(buffer);
            fprintf(stdout, "<- [][%" PRIu64 "][%" PRIu64 "][%" PRIu64 "]\n", server_send_timestamp, server_recv_timestamp, client_send_timestamp);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
