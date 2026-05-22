#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8082
#define BUFFER_SIZE 4096

typedef enum { STAGE_FRONT, STAGE_MIDDLE, STAGE_EXIT } FilterStage;

typedef struct {
    float pue;
    unsigned long heavy_dirt_pruned;
    unsigned long clean_pow_bytes;
    int system_state; // 0 = Standby, 1 = Green Active
    double food_energy_required; // Analogy parameter: 2.0kg vs 0.2kg
} ThreeStageMatrix;

volatile int is_matrix_active = 1;
pthread_mutex_t stage_lock = PTHREAD_MUTEX_INITIALIZER;
ThreeStageMatrix marduk_stack = {1.24f, 57169, 1052, 1, 2.0}; // Default traditional values matching your image [19]

// In-Memory Three-Stage Pipeline Processor
void process_three_tier_filter(char *raw_packet, int length) {
    // STAGE 1: FRONT SLUICE-BENCH (Dealing with lots of dirt)
    if (length > 1460 || strstr(raw_packet, "MALICIOUS_SURGE") != NULL) {
        marduk_stack.heavy_dirt_pruned += length;
        return; // Heavy dirt dropped instantly at the gate
    }

    // STAGE 2: MIDDLE SLUICE-BENCH (Less dirt, pure validation tracking)
    marduk_stack.clean_pow_bytes += length;

    // STAGE 3: EXIT SLUICE-BENCH (Output formatting into text/html strings)
    if (marduk_stack.system_state == 1) {
        // Green Energy Profile Active: 12km running on 0.2kg food energy
        marduk_stack.pue = 1.001f;
        marduk_stack.food_energy_required = 0.2;
    } else {
        // Traditional Power Profile: 12km running on 2.0kg food energy
        marduk_stack.pue = 1.24f;
        marduk_stack.food_energy_required = 2.0;
    }
}

void* broker_kernel(void* arg) {
    (void)arg;
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1, addr_len = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0 || listen(server_fd, 20) < 0) {
        pthread_exit(NULL);
    }

    while (is_matrix_active) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addr_len);
        if (client_socket < 0) continue;

        char rx_buf[BUFFER_SIZE] = {0};
        int read_bytes = recv(client_socket, rx_buf, BUFFER_SIZE - 1, 0);

        // CORS browser pre-flight verification
        if (strncmp(rx_buf, "OPTIONS", 7) == 0) {
            char cors[] = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
            send(client_socket, cors, strlen(cors), 0);
            close(client_socket);
            continue;
        }

        pthread_mutex_lock(&stage_lock);

        if (strstr(rx_buf, "/stall") != NULL) {
            marduk_stack.system_state = 0; // Standby traditional baseline
        } else if (strstr(rx_buf, "/start_green") != NULL) {
            marduk_stack.system_state = 1; // Engage optimized green energy consumption
        }

        if (read_bytes > 0) {
            process_three_tier_filter(rx_buf, read_bytes);
        }

        char payload[512];
        snprintf(payload, sizeof(payload), 
                 "{\"pue\": %.3f, \"pruned\": %lu, \"pow\": %lu, \"state\": %d, \"food\": %.2f}", 
                 marduk_stack.pue, marduk_stack.heavy_dirt_pruned, marduk_stack.clean_pow_bytes, marduk_stack.system_state, marduk_stack.food_energy_required);
        
        pthread_mutex_unlock(&stage_lock);

        char response[1024];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n%s", payload);

        send(client_socket, response, strlen(response), 0);
        close(client_socket);
    }
    close(server_fd);
    pthread_exit(NULL);
}

int main(void) {
    pthread_t net_thread;
    pthread_create(&net_thread, NULL, broker_kernel, NULL);
    getchar();
    is_matrix_active = 0;
    pthread_join(net_thread, NULL);
    pthread_mutex_destroy(&stage_lock);
    return 0;
}
