#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

#define SAT_STREAM_HOST "earthquake.usgs.gov"
#define SAT_STREAM_PORT 80
#define MARDUK_CORE_PORT 8082
#define VOLATILE_BUFFER_SIZE 4096

int main(void) {
    int sat_socket, local_core_socket;
    struct hostent *satellite_host;
    struct sockaddr_in satellite_addr, local_core_addr;

    // Production HTTP/1.1 Frame explicitly designed for orbital telemetry extraction mirrors
    char downlink_request[] = 
        "GET /earthquakes/feed/v1.0/summary/all_hour.geojson HTTP/1.1\r\n"
        "Host: earthquake.usgs.gov\r\n"
        "User-Agent: MardukSatelliteDownlinkBase/3.0 (Space-Ingress Node)\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n";

    printf("====================================================\n");
    printf("     SA-MARDUKH SATELLITE TRANSMISSION DOWNLINK     \n");
    printf("====================================================\n");

    while (1) {
        printf("[Satellite Ingress] Syncing DNS orbital path for: %s...\n", SAT_STREAM_HOST);
        satellite_host = gethostbyname(SAT_STREAM_HOST);
        if (satellite_host == NULL) {
            sleep(5);
            continue;
        }

        sat_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (sat_socket < 0) { sleep(2); continue; }

        memset(&satellite_addr, 0, sizeof(satellite_addr));
        satellite_addr.sin_family = AF_INET;
        memcpy(&satellite_addr.sin_addr.s_addr, satellite_host->h_addr, satellite_host->h_length);
        satellite_addr.sin_port = htons(SAT_STREAM_PORT);

        printf("[Satellite Ingress] Locking telemetry link to orbital transponder arrays...\n");
        if (connect(sat_socket, (struct sockaddr *)&satellite_addr, sizeof(satellite_addr)) < 0) {
            close(sat_socket);
            sleep(5);
            continue;
        }

        printf("[Satellite Ingress] Link locked! Streaming raw down-link byte envelopes...\n");
        send(sat_socket, downlink_request, strlen(downlink_request), 0);

        // Open local socket path straight into the rear core supervisor
        local_core_socket = socket(AF_INET, SOCK_STREAM, 0);
        local_core_addr.sin_family = AF_INET;
        local_core_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        local_core_addr.sin_port = htons(MARDUK_CORE_PORT);

        if (connect(local_core_socket, (struct sockaddr *)&local_core_addr, sizeof(local_core_addr)) >= 0) {
            char download_register[VOLATILE_BUFFER_SIZE];
            int chunk_bytes = 0;

            // STAGE 1: FRONT SLUICE-BENCH NOISE FILTERING
            // Read incoming telemetry chunks directly from space link into volatile memory registers
            while ((chunk_bytes = recv(sat_socket, download_register, VOLATILE_BUFFER_SIZE - 1, 0)) > 0) {
                download_register[chunk_bytes] = '\0';
                
                // Pack metrics data inside an isolated transmission envelope flag frame
                char envelope_frame[VOLATILE_BUFFER_SIZE + 128];
                snprintf(envelope_frame, sizeof(envelope_frame),
                         "POST /SATELLITE_DOWNLINK HTTP/1.1\r\n"
                         "Host: 127.0.0.1\r\n"
                         "Content-Length: %d\r\n\r\n"
                         "%s", chunk_bytes, download_register);
                
                // Stream it directly to the local C core engine
                send(local_core_socket, envelope_frame, strlen(envelope_frame), 0);
            }
            close(local_core_socket);
            printf("[Sluice Link] Volatile down-link stream batch cycle complete.\n");
        } else {
            fprintf(stderr, "[Warning] sa_sluice_core engine offline on local port 8082.\n");
        }

        close(sat_socket);
        printf("[Satellite Ingress] Ingestion cycle complete. Next pass-through in 10s...\n\n");
        sleep(10);
    }
    return 0;
}
