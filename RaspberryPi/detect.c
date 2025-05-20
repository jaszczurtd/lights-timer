#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include <cjson/cJSON.h>

#define DISCOVER_PORT 12345
#define DISCOVER_MSG "AQUA_DISCOVER"
#define BROADCAST_IP "255.255.255.255"
#define MAX_BUFFER_SIZE 256

// Struktura przechowująca informacje o urządzeniu
typedef struct {
    char mac[18];
    char ip[16];
    char hostName[256];
    int switches;
} DeviceInfo;

void *discovery_thread(void *arg);
void send_broadcast_message(int sock);
void handle_device_response(char *message);
void create_json(DeviceInfo devices[], int device_count);

int main() {
    int sock;
    struct sockaddr_in server_addr;
    struct timeval timeout;

    // Tworzenie soketu UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Ustawienie soketu do wysyłania broadcastów
    int broadcast = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("Setting socket option failed");
        exit(EXIT_FAILURE);
    }

    // Ustawienie timeoutu dla soketu (2 sekundy)
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Setting socket timeout failed");
        exit(EXIT_FAILURE);
    }

    // Adres docelowy dla broadcastu
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DISCOVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
	
	// Dodaj BIND do portu 12345
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(DISCOVER_PORT);  // Stały port 12345
    bind_addr.sin_addr.s_addr = INADDR_ANY;     // Nasłuchuj na wszystkich interfejsach

    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }	

	printf("Oczekuje na odpowiedzi na porcie %d...\n", DISCOVER_PORT);

    // Pętla wykrywania urządzeń co sekundę
    while (1) {
        send_broadcast_message(sock);
        sleep(1);
    }

    close(sock);
    return 0;
}

void send_broadcast_message(int sock) {
    struct sockaddr_in server_addr;
    char send_buffer[MAX_BUFFER_SIZE];

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DISCOVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // Wysyłanie wiadomości broadcastowej
    strcpy(send_buffer, DISCOVER_MSG);
    if (sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Send failed");
        return;
    }

    // Odbieranie odpowiedzi
    char recv_buffer[MAX_BUFFER_SIZE];
	socklen_t addr_len = sizeof(server_addr);
	while (1) {
        ssize_t bytes_received = recvfrom(sock, recv_buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // Koniec timeoutu
            perror("Receive failed");
            break;
        }

        recv_buffer[bytes_received] = '\0';
        handle_device_response(recv_buffer);
    }

}

void handle_device_response(char *message) {
    // Jeśli odpowiedź zaczyna się od "AQUA_FOUND|"
    if (strncmp(message, "AQUA_FOUND|", 11) == 0) {
		printf("message: %s\n", message);
		
        char *mac = strtok(message + 11, "|");
        char *ip = strtok(NULL, "|");
        char *hostName = strtok(NULL, "|");
        char *switches_str = strtok(NULL, "|") + strlen("SW:");

        if (mac && ip && hostName && switches_str) {
            int switches = atoi(switches_str);

            // Tworzenie struktury DeviceInfo
            DeviceInfo device;
            strncpy(device.mac, mac, sizeof(device.mac));
            strncpy(device.ip, ip, sizeof(device.ip));
            strncpy(device.hostName, hostName, sizeof(device.hostName));
            device.switches = switches;

            // Dodaj urządzenie do JSON
            static DeviceInfo devices[10];
            static int device_count = 0;
            if (device_count < 10) {
                devices[device_count++] = device;
                create_json(devices, device_count);
            }
        }
    }
}

void create_json(DeviceInfo devices[], int device_count) {
    cJSON *root = cJSON_CreateObject();
    cJSON *device_list = cJSON_CreateArray();

    for (int i = 0; i < device_count; i++) {
        cJSON *device_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(device_obj, "mac", devices[i].mac);
        cJSON_AddStringToObject(device_obj, "ip", devices[i].ip);
        cJSON_AddStringToObject(device_obj, "hostName", devices[i].hostName);
        cJSON_AddNumberToObject(device_obj, "switches", devices[i].switches);
        cJSON_AddItemToArray(device_list, device_obj);
    }

    cJSON_AddItemToObject(root, "devices", device_list);

    // Serializacja JSON do stringa
    char *json_str = cJSON_Print(root);
    printf("Devices JSON:\n%s\n", json_str);

    // Zwolnienie pamięci
    free(json_str);
    cJSON_Delete(root);
}
