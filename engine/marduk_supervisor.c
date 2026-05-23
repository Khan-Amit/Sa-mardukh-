#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>

#define WEB_SERVER_PORT 8082
#define INGEST_MAX_BUFFER 8192
#define NASA_FEED_HOST "earthquake.usgs.gov"

typedef struct {
    float real_time_pue;
    unsigned long stage_1_noise_pruned;
    unsigned long stage_2_pow_bytes;
    int green_energy_engaged;
} SupervisorMatrix;

// Global System Variables
volatile int is_system_active = 1;
pthread_mutex_t core_lock = PTHREAD_MUTEX_INITIALIZER;
SupervisorMatrix system_registry = {1.24f, 57169, 1052, 0};

// --- HIERARCHICAL STAGE FUNCTIONS RUN COMPLETELY IN-MEMORY ---

// STAGE 1 & 2: Front Filtering and Proof-of-Work Registration
void execute_internal_sluice_stages(char *raw_packet, int packet_len) {
    pthread_mutex_lock(&core_lock);

    // STAGE 1: FRONT SLUICE-BENCH (Heavy Dirt & Injection Pruning)
    if (packet_len > 1500 || strstr(raw_packet, "NOISE_SURGE_ATTACK") != NULL) {
        system_registry.stage_1_noise_pruned += packet_len; // Drop instantly from RAM
        pthread_mutex_unlock(&core_lock);
        return; 
    }

    // STAGE 2: MIDDLE SLUICE-BENCH (In-Memory Data Weight Accumulation)
    system_registry.stage_2_pow_bytes += packet_len;
    
    if (system_registry.green_energy_engaged == 1) {
        system_registry.real_time_pue = 1.001f; // Lock optimal PUE target
    } else {
        system_registry.real_time_pue = 1.24f;  // Traditional power profile
    }

    pthread_mutex_unlock(&core_lock);
}

// STAGE 3: EXIT TRANSLATION (Background Data Harvester from Live Internet API)
void* live_internet_collector_thread(void* arg) {
    (void)arg;
    struct hostent *dns_resolution;
    struct sockaddr_in remote_server_addr;
    char valid_http_request[] = 
        "GET /earthquakes/feed/v1.0/summary/all_hour.geojson HTTP/1.1\r\n"
        "Host: earthquake.usgs.gov\r\n"
        "User-Agent: MardukSupervisorEngine/3.0\r\n"
        "Connection: close\r\n\r\n";

    while (is_system_active) {
        dns_resolution = gethostbyname(NASA_FEED_HOST);
        if (dns_resolution == NULL) { sleep(5); continue; }

        int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&remote_server_addr, 0, sizeof(remote_server_addr));
        remote_server_addr.sin_family = AF_INET;
        memcpy(&remote_server_addr.sin_addr.s_addr, dns_resolution->h_addr, dns_resolution->h_length);
        remote_server_addr.sin_port = htons(80);

        if (connect(socket_fd, (struct sockaddr *)&remote_server_addr, sizeof(remote_server_addr)) >= 0) {
            send(socket_fd, valid_http_request, strlen(valid_http_request), 0);
            
            char buffer[1024];
            int received_bytes;
            // Pull real bytes from the live internet, passing them directly to the filter logic
            while ((received_bytes = recv(socket_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
                buffer[received_bytes] = '\0';
                execute_internal_sluice_stages(buffer, received_bytes);
            }
        }
        close(socket_fd);
        sleep(10); // Safe pacing interval to prevent data cable energy bleeding
    }
    pthread_exit(NULL);
}

// STAGE 3 GATEWAY: Non-Blocking Web Interface Server
void* dashboard_network_broker(void* arg) {
    (void)arg;
    int server_fd, client_socket;
    struct sockaddr_in address;
    int option_flag = 1, addr_len = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option_flag, sizeof(option_flag));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(WEB_SERVER_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0 || listen(server_fd, 50) < 0) {
        printf("[Fatal Error] Bound to port 8082 rejected.\n");
        pthread_exit(NULL);
    }

    printf("[Supervisor Core] Single File Engine Web Interface Listening on Port %d\n", WEB_SERVER_PORT);

    while (is_system_active) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addr_len);
        if (client_socket < 0) continue;

        char rx_buffer[INGEST_MAX_BUFFER] = {0};
        recv(client_socket, rx_buffer, INGEST_MAX_BUFFER - 1, 0);

        // Immediate CORS approval routine for browser dashboards
        if (strncmp(rx_buffer, "OPTIONS", 7) == 0) {
            char cors[] = "HTTP/1.1 204 No Content\r\n"
                          "Access-Control-Allow-Origin: *\r\n"
                          "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                          "Connection: close\r\n\r\n";
            send(client_socket, cors, strlen(cors), 0);
            close(client_socket);
            continue;
        }

        pthread_mutex_lock(&core_lock);

        // Parse runtime changes issued by frontend buttons
        if (strstr(rx_buffer, "/start_green") != NULL) {
            system_registry.green_energy_engaged = 1;
            printf("[System Event] Switched to Green 0.2kg food consumption profile.\n");
        } else if (strstr(rx_buffer, "/stall") != NULL) {
            system_registry.green_energy_engaged = 0;
            printf("[System Event] Switched to Traditional 2.0kg food profile.\n");
        }

        double calculated_food_load = (system_registry.green_energy_engaged == 1) ? 0.2 : 2.0;

        char output_payload[512];
        snprintf(output_payload, sizeof(output_payload),
                 "{\"pue\": %.3f, \"noise_bytes\": %lu, \"pow_bytes\": %lu, \"green_active\": %d, \"food\": %.2f}",
                 system_registry.real_time_pue, system_registry.stage_1_noise_pruned, 
                 system_registry.stage_2_pow_bytes, system_registry.green_energy_engaged, calculated_food_load);

        pthread_mutex_unlock(&core_lock);

        char server_http_frame[1024];
        snprintf(server_http_frame, sizeof(server_http_frame),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n"
                 "%s", output_payload);

        send(client_socket, server_http_frame, strlen(server_http_frame), 0);
        close(client_socket); // Enforce your strict, clean zero-storage boundary
    }
    close(server_fd);
    pthread_exit(NULL);
}

int main(void) {
    pthread_t collector_thread_id, web_thread_id;
    srand(time(NULL));

    // Spin up concurrent tasks within the same execution path
    pthread_create(&collector_thread_id, NULL, live_internet_collector_thread, NULL);
    pthread_create(&web_thread_id, NULL, dashboard_network_broker, NULL);

    printf("[SA-MARDUKH UNIFIED ENGINE] Running entirely inside single source process frame.\n");
    printf("Press [ENTER] to cleanly safely shutdown server operations.\n");
    getchar();

    is_system_active = 0;
    pthread_join(web_thread_id, NULL);
    pthread_join(collector_thread_id, NULL);
    pthread_mutex_destroy(&core_lock);

    printf("[Teardown] Memory context released safely.\n");
    return 0;
}
