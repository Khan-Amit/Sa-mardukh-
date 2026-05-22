#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define REMOTE_SOURCE "earthquake.usgs.gov"
#define CORE_TARGET_PORT 8082

int main(void) {
    int remote_fd, local_core_fd;
    struct hostent *dns_record;
    struct sockaddr_in remote_server_addr, local_core_addr;
    char http_get_request[] = "GET /earthquakes/feed/v1.0/summary/all_hour.geojson HTTP/1.1\r\n"
                             "Host: earthquake.usgs.gov\r\n"
                             "Connection: close\r\n\r\n";

    while (1) {
        dns_record = gethostbyname(REMOTE_SOURCE);
        if (dns_record == NULL) { sleep(5); continue; }

        remote_fd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&remote_server_addr, 0, sizeof(remote_server_addr));
        remote_server_addr.sin_family = AF_INET;
        memcpy(&remote_server_addr.sin_addr.s_addr, dns_record->h_addr, dns_record->h_length);
        remote_server_addr.sin_port = htons(80);

        if (connect(remote_fd, (struct sockaddr *)&remote_server_addr, sizeof(remote_server_addr)) < 0) {
            close(remote_fd); sleep(5); continue;
        }

        send(remote_fd, http_get_request, strlen(http_get_request), 0);

        local_core_fd = socket(AF_INET, SOCK_STREAM, 0);
        local_core_addr.sin_family = AF_INET;
        local_core_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        local_core_addr.sin_port = htons(CORE_TARGET_PORT);

        if (connect(local_core_fd, (struct sockaddr *)&local_core_addr, sizeof(local_core_addr)) >= 0) {
            char socket_stream_byte;
            while (recv(remote_fd, &socket_stream_byte, 1, 0) > 0) {
                // Pipe live packet streams straight over local loopback connection interface
                send(local_core_fd, &socket_stream_byte, 1, 0);
            }
            close(local_core_fd);
        }
        close(remote_fd);
        sleep(2); // Ingestion check cycle tracking
    }
    return 0;
}
