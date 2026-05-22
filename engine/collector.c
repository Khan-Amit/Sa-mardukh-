#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

// 100% Valid, Free, and Public Live Telemetry Source Network Details
#define REMOTE_API_HOST "earthquake.usgs.gov"
#define REMOTE_API_PORT 80
#define CORE_TARGET_PORT 8082
#define INGEST_TEMP_BUFFER 4096

int main(void) {
    int remote_socket_fd, local_core_fd;
    struct hostent *dns_resolution;
    struct sockaddr_in remote_server_addr, local_core_addr;

    // Production-Grade HTTP/1.1 Request Payload Header Frame
    // Explicitly defines the host and client signatures so public servers do not reject the connection
    char http_valid_request[] = 
        "GET /earthquakes/feed/v1.0/summary/all_hour.geojson HTTP/1.1\r\n"
        "Host: earthquake.usgs.gov\r\n"
        "User-Agent: MardukCollectorSubsystem/2.0 (Unix; Isolated Device Lab)\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n";

    printf("====================================================\n");
    printf("     SA-MARDUKH VALID EXTERNAL API HARVESTER        \n");
    printf("====================================================\n");

    while (1) {
        printf("[API Pipeline] Resolving Domain Name: %s...\n", REMOTE_API_HOST);
        
        dns_resolution = gethostbyname(REMOTE_API_HOST);
        if (dns_resolution == NULL) {
            fprintf(stderr, "[DNS Warning] Server target unreachable. Re-polling in 5s...\n");
            sleep(5);
            continue;
        }

        // Initialize target remote stream connection endpoint socket
        remote_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (remote_socket_fd < 0) {
            sleep(2);
            continue;
        }

        memset(&remote_server_addr, 0, sizeof(remote_server_addr));
        remote_server_addr.sin_family = AF_INET;
        memcpy(&remote_server_addr.sin_addr.s_addr, dns_resolution->h_addr, dns_resolution->h_length);
        remote_server_addr.sin_port = htons(REMOTE_API_PORT);

        printf("[API Pipeline] Connecting to live planetary sensor telemetry node...\n");
        if (connect(remote_socket_fd, (struct sockaddr *)&remote_server_addr, sizeof(remote_server_addr)) < 0) {
            fprintf(stderr, "[API Error] Remote endpoint rejected connection link: %s\n", strerror(errno));
            close(remote_socket_fd);
            sleep(5);
            continue;
        }

        printf("[API Pipeline] Handshake successful! Requesting valid telemetry block data...\n");
        send(remote_socket_fd, http_valid_request, strlen(http_valid_request), 0);

        // Instantly establish a pipeline to pass data over to your local Sluice Core Server
        local_core_fd = socket(AF_INET, SOCK_STREAM, 0);
        local_core_addr.sin_family = AF_INET;
        local_core_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        local_core_addr.sin_port = htons(CORE_TARGET_PORT);

        if (connect(local_core_fd, (struct sockaddr *)&local_core_addr, sizeof(local_core_addr)) >= 0) {
            char chunk_register[INGEST_TEMP_BUFFER];
            int received_chunk_bytes = 0;

            printf("[Sluice Link] Inbound channel open! Feeding raw internet data strings to backend...\n");

            // Pull live streaming data lines down from the authentic web endpoint
            while ((received_chunk_bytes = recv(remote_socket_fd, chunk_register, INGEST_TEMP_BUFFER - 1, 0)) > 0) {
                chunk_register[received_chunk_bytes] = '\0';
                
                // Wrap data inside a clean internal protocol string header
                char internal_transmission_frame[INGEST_TEMP_BUFFER + 128];
                snprintf(internal_transmission_frame, sizeof(internal_transmission_frame),
                         "POST /LIVE_STREAM HTTP/1.1\r\n"
                         "Host: 127.0.0.1\r\n"
                         "Content-Length: %d\r\n\r\n"
                         "%s", received_chunk_bytes, chunk_register);
                
                // Dispatch the real-world byte payload right into your sa_sluice_core
                send(local_core_fd, internal_transmission_frame, strlen(internal_transmission_frame), 0);
            }
            close(local_core_fd);
            printf("[Sluice Link] Volatile transmission batch completed successfully.\n");
        } else {
            fprintf(stderr, "[Local Link Warning] Local sa_sluice_core offline on port 8082. Open it first.\n");
        }

        close(remote_socket_fd);
        printf("[API Pipeline] Ingestion interval complete. Re-polling infrastructure loop in 10s...\n\n");
        sleep(10); // Safe 10-second request pacing interval to keep network channels clean
    }

    return 0;
}
