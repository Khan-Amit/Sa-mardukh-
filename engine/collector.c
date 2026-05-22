#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

// 100% Free, Public, and Unauthenticated Real-World Global API Nodes
#define NASA_NOAA_MIRROR_HOST "earthquake.usgs.gov"
#define REMOTE_HTTP_PORT     80
#define LOCAL_SLUICE_PORT    8082
#define INGEST_BUFFER_SIZE   4096

int main(void) {
    int remote_net_fd, local_sluice_fd;
    struct hostent *dns_record;
    struct sockaddr_in remote_server_addr, local_core_addr;

    // Production-Grade HTTP/1.1 Request Payload Frame
    // Valid headers ensure the public CDN does not drop your cell phone network requests with a 403 error
    char live_api_request[] = 
        "GET /earthquakes/feed/v1.0/summary/all_hour.geojson HTTP/1.1\r\n"
        "Host: earthquake.usgs.gov\r\n"
        "User-Agent: SaliimAhmedCollector/3.0 (Unix; Standard Mobile Network Pipeline)\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n";

    printf("====================================================\n");
    printf("     MARDUK_F REAL-WORLD INTERNET DATA COLLECTOR    \n");
    printf("====================================================\n");

    while (1) {
        printf("[Internet Ingress] Resolving public live feed domain: %s...\n", NASA_NOAA_MIRROR_HOST);
        
        dns_record = gethostbyname(NASA_NOAA_MIRROR_HOST);
        if (dns_record == NULL) {
            fprintf(stderr, "[Network Delay] DNS lookup failed. Device offline or recycling. Retrying in 5s...\n");
            sleep(5);
            continue;
        }

        // 1. Open standard TCP network interface socket line to the public web
        remote_net_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (remote_net_fd < 0) {
            sleep(2);
            continue;
        }

        memset(&remote_server_addr, 0, sizeof(remote_server_addr));
        remote_server_addr.sin_family = AF_INET;
        memcpy(&remote_server_addr.sin_addr.s_addr, dns_record->h_addr, dns_record->h_length);
        remote_server_addr.sin_port = htons(REMOTE_HTTP_PORT);

        printf("[Internet Ingress] Connecting to public climate/planetary stream server...\n");
        if (connect(remote_net_fd, (struct sockaddr *)&remote_server_addr, sizeof(remote_server_addr)) < 0) {
            fprintf(stderr, "[Internet Error] Connection link rejected by remote host: %s\n", strerror(errno));
            close(remote_net_fd);
            sleep(5);
            continue;
        }

        printf("[Internet Ingress] Connected! Fetching live public data arrays...\n");
        send(remote_net_fd, live_api_request, strlen(live_api_request), 0);

        // 2. Open local communication path into your second Sluice-Bench processing engine
        local_sluice_fd = socket(AF_INET, SOCK_STREAM, 0);
        local_core_addr.sin_family = AF_INET;
        local_core_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        local_core_addr.sin_port = htons(LOCAL_SLUICE_PORT);

        if (connect(local_sluice_fd, (struct sockaddr *)&local_core_addr, sizeof(local_core_addr)) >= 0) {
            char dynamic_chunk_register[INGEST_BUFFER_SIZE];
            int received_stream_bytes = 0;

            printf("[Pipeline Flow] Sluice channel synchronized. Streaming raw text bytes to backend...\n");

            // Pull live streaming characters down from the real internet source
            while ((received_stream_bytes = recv(remote_net_fd, dynamic_chunk_register, INGEST_BUFFER_SIZE - 1, 0)) > 0) {
                dynamic_chunk_register[received_stream_bytes] = '\0';
                
                // Wrap the live web data bytes into an internal transmission framework envelope
                char transmission_envelope[INGEST_BUFFER_SIZE + 128];
                snprintf(transmission_envelope, sizeof(transmission_envelope),
                         "POST /LIVE_STREAM HTTP/1.1\r\n"
                         "Host: 127.0.0.1\r\n"
                         "Content-Length: %d\r\n\r\n"
                         "%s", received_stream_bytes, dynamic_chunk_register);
                
                // Deliver the real-world byte weight directly to your backend C processing loops
                send(local_sluice_fd, transmission_envelope, strlen(transmission_envelope), 0);
            }
            close(local_sluice_fd);
            printf("[Pipeline Flow] Volatile network transaction complete. Registers flushed.\n");
        } else {
            fprintf(stderr, "[Pipeline Warning] Sluice-Bench backend offline on port 8082. Launch it first.\n");
        }

        close(remote_net_fd);
        printf("[Internet Ingress] Batch complete. Restoring collection timers for 10 seconds...\n\n");
        sleep(10); // Maintain a stable, non-blocking 10s request pacing interval to prevent data cable overload
    }

    return 0;
}
