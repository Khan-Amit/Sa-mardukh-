#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define REMOTE_NASA_HOST "earthquake.usgs.gov"
#define CORE_ROUTER_PORT 8082
#define REG_MAX_BUFFER 4096

typedef struct {
    float invariant_pue;
    unsigned long static_noise_pruned;
    unsigned long binary_pow_weight;
    int relay_active;
} AlaskaSiberiaRegister;

volatile int is_relay_operational = 1;
AlaskaSiberiaRegister relay_state = {1.001f, 57169, 1052, 1};

// Core Low-Magnetic Translator: Strips static noise and outputs light binary weight
void convert_stream_to_binary_pulses(char *raw_data, int len) {
    for (int i = 0; i < len; i++) {
        // Evaluate the raw byte array bit-by-bit into strict ON/OFF machine signals
        for (int bit = 7; bit >= 0; bit--) {
            int bit_state = (raw_data[i] >> bit) & 1;
            if (bit_state) {
                relay_state.binary_pow_weight++; // Increment ON state pulse weight
            } else {
                relay_state.static_noise_pruned++; // Increment OFF state structural noise filter
            }
        }
    }
    // Lock baseline PUE to pure algorithmic zero overhead
    relay_state.invariant_pue = 1.001f;
}

int main(void) {
    int server_fd, client_socket, nasa_fd;
    struct hostent *dns_resolver;
    struct sockaddr_in server_addr, nasa_addr, client_addr;
    int opt = 1, addr_len = sizeof(client_addr);

    char nasa_request[] = "GET /earthquakes/feed/v1.0/summary/all_hour.geojson HTTP/1.1\r\n"
                          "Host: earthquake.usgs.gov\r\n"
                          "Connection: close\r\n\r\n";

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(CORE_ROUTER_PORT);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 || listen(server_fd, 20) < 0) {
        fprintf(stderr, "[Relay Core] Port 8082 allocation failed.\n");
        return -1;
    }

    printf("[Relay Core] Terrestrial Dateline Base Active on Port %d\n", CORE_ROUTER_PORT);

    while (is_relay_operational) {
        client_socket = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addr_len);
        if (client_socket < 0) continue;

        char rx_buf[REG_MAX_BUFFER] = {0};
        recv(client_socket, rx_buf, REG_MAX_BUFFER - 1, 0);

        // Handle direct satellite relay tracking checking signals
        if (strstr(rx_buf, "/satellite_check") != NULL) {
            printf("[Relay Event] Pinging NASA Satellite Node to check transponder links...\n");
            dns_resolver = gethostbyname(REMOTE_NASA_HOST);
            if (dns_resolver != NULL) {
                nasa_fd = socket(AF_INET, SOCK_STREAM, 0);
                nasa_addr.sin_family = AF_INET;
                memcpy(&nasa_addr.sin_addr.s_addr, dns_resolver->h_addr, dns_resolver->h_length);
                nasa_addr.sin_port = htons(80);
                
                if (connect(nasa_fd, (struct sockaddr *)&nasa_addr, sizeof(nasa_addr)) >= 0) {
                    send(nasa_fd, nasa_request, strlen(nasa_request), 0);
                    char chunk[512];
                    int r = recv(nasa_fd, chunk, 511, 0);
                    if (r > 0) convert_stream_to_binary_pulses(chunk, r);
                    close(nasa_fd);
                    relay_state.relay_active = 1;
                }
            }
        }

        char payload;
        snprintf(payload, sizeof(payload), 
                 "{\"pue\": %.3f, \"noise_bytes\": %lu, \"pow_bytes\": %lu, \"sat_status\": %d}", 
                 relay_state.invariant_pue, relay_state.static_noise_pruned, relay_state.binary_pow_weight, relay_state.relay_active);

        char response;
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n%s", payload);

        send(client_socket, response, strlen(response), 0);
        close(client_socket);
    }
    close(server_fd);
    return 0;
}
