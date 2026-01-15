#include "provider.h"

DeviceInfo *devices = NULL;
int device_count;
volatile bool keep_running = false;
char username[MAX_INPUT_SIZE], password[MAX_INPUT_SIZE];
char path[MAX_BUFFER_SIZE];
struct mosquitto *mosq = NULL;

void handle_sigint(int sig) {
    keep_running = 0;
}

void get_password(char *buf, size_t buflen) {
    struct termios oldt, newt;
    printf("password: ");
    fflush(stdout);

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt; newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    fgets(buf, buflen, stdin);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");

    buf[strcspn(buf, "\n")] = '\0'; // usuń \n
}

void delete_credentials(void) {
    snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), AUTH_PATH);
    if (!remove(path) != 0) {
        fprintf(stderr, "cannot remove %s\n", path);
    }    
}

int read_or_prompt_credentials(char *username, char *password) {
    snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), AUTH_PATH);

    FILE *f = fopen(path, "r");
    if (f) {
        fgets(username, MAX_INPUT_SIZE, f);
        fgets(password, MAX_INPUT_SIZE, f);
        fclose(f);
        username[strcspn(username, "\n")] = '\0';
        password[strcspn(password, "\n")] = '\0';
        return 0;
    }

    // prompt
    printf("login: ");
    fgets(username, MAX_INPUT_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0';

    get_password(password, MAX_INPUT_SIZE);

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "fopen error: %s\n", path);
        return -1;
    }
    fprintf(f, "%s\n%s\n", username, password);
    fclose(f);
    chmod(path, 0600); // tylko dla właściciela

    return 0;
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
    (void)mosq;
    (void)obj;
    (void)rc;
    // configure auto-reconnect delays if you want:
    mosquitto_reconnect_delay_set(mosq, 1, 20, true);
}

bool mosquitto_init(void) {
    mosquitto_lib_init();

    mosq = mosquitto_new(MQTT_CLIENT_ID, true, NULL);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_username_pw_set(mosq, username, password);

    if (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, MQTT_KEEPALIVE) != MOSQ_ERR_SUCCESS) {
        mosquitto_quit();
        return false;
    }
    if(mosquitto_loop_start(mosq) != MOSQ_ERR_SUCCESS) {
        mosquitto_quit();
        return false;
    }

    return true;
}

void mosquitto_quit(void) {
    if(mosq != NULL) {
        mosquitto_loop_stop(mosq, true);
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        mosq = NULL;
    }
    mosquitto_lib_cleanup();
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    struct timeval timeout;
    DeviceInfo *curr;

    if (read_or_prompt_credentials(username, password) != 0) {
        fprintf(stderr, "cannot get credentials\n");
        delete_credentials();
        exit(EXIT_FAILURE);
    }

    if(!strlen(username)) {
        fprintf(stderr, "username cannot be empty\n");
        delete_credentials();
        exit(EXIT_FAILURE);
    }
    if(!strlen(password)) {
        fprintf(stderr, "password cannot be empty\n");
        delete_credentials();
        exit(EXIT_FAILURE);
    }

    if(!mosquitto_init()) {
        fprintf(stderr, "MQTT connection failed\n");
        exit(EXIT_FAILURE);
    }

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
        close(sock);
        mosquitto_lib_cleanup();
        exit(EXIT_FAILURE);
    }	

    signal(SIGINT, handle_sigint);
    signal(SIGPIPE, SIG_IGN);

    printf("Aqua devices provider ver. %s\n", VERSION);
	printf("Waiting for devices to show up on port %d...\n", DISCOVER_PORT);

    keep_running = true;
    while (keep_running) {
        send_broadcast_message(sock);
        sleep(BROADCAST_SECONDS_DELAY);
        if(!keep_running) {
            break;
        }

        bool not_ok = false;
        DeviceInfo *prev = NULL;
        curr = devices;
        while (curr) {
            if(!keep_running) {
                break;
            }

            if(!is_device_alive_tcp(curr->ip, DISCOVER_PORT)) {
                printf("device %s is not responding!\n", curr->hostName);
                DeviceInfo *to_free = curr;
                if(prev) {
                    prev->next = curr->next;
                } else {
                    devices = curr->next;
                }
                curr = curr->next;
                free(to_free);
                not_ok = true;

            } else {
                prev = curr;
                curr = curr->next;
            }
        }

        if(not_ok) {
            notify_mqtt();
        }
    }

    printf("exiting & cleaning up\n");

    close(sock);

    curr = devices;
    while (curr) {
        DeviceInfo *next = curr->next;
        free(curr);
        curr = next;
    }

    mosquitto_quit();

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
	while (keep_running) {
        ssize_t bytes_received = recvfrom(sock, recv_buffer, MAX_BUFFER_SIZE - 1, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; // Koniec timeoutu
            perror("Receive failed");
            break;
        }

        recv_buffer[bytes_received] = '\0';
        handle_device_response(recv_buffer);
    }
}

int is_device_alive_tcp(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in addr;
    struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return 0;

    // Ustaw non-blocking
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int result = connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (result == 0) {
        close(sockfd);
        return 1; // Połączenie od razu OK
    } else if (errno != EINPROGRESS) {
        close(sockfd);
        return 0; // Błąd krytyczny
    }

    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sockfd, &writefds);

    // Czekaj max 1 sekundę
    if (select(sockfd + 1, NULL, &writefds, NULL, &timeout) > 0) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        close(sockfd);
        return so_error == 0 || so_error == ECONNREFUSED;
    }

    close(sockfd);
    return 0;
}

void handle_device_response(char *message) {
    // Jeśli odpowiedź zaczyna się od "AQUA_FOUND|"
    if (strncmp(message, AQUA_FOUND, strlen(AQUA_FOUND)) == 0) {
        char *mac = strtok(message + strlen(AQUA_FOUND), "|");
        char *ip = strtok(NULL, "|");
        char *hostName = strtok(NULL, "|");
        char *switches_str = strtok(NULL, "|") + strlen("SW:");
        
        if (!mac || !ip || !hostName || !switches_str) return;

        int switches = atoi(switches_str);

        DeviceInfo *d = devices;
        while (d) {
            if (strncmp(d->mac, mac, sizeof(d->mac)) == 0) {
                return; // Duplikat, ignorujemy
            }
            d = d->next;
        }

		printf("new device found: %s\n", hostName);
		
        // Nowe urządzenie
        DeviceInfo *new_device = calloc(1, sizeof(DeviceInfo));
        if (!new_device) {
            fprintf(stderr, "out of memory!\n");
            return;
        }

        strncpy(new_device->mac, mac, sizeof(new_device->mac) - 1);
        strncpy(new_device->ip, ip, sizeof(new_device->ip) - 1);
        strncpy(new_device->hostName, hostName, sizeof(new_device->hostName) - 1);
        new_device->switches = switches;
        new_device->next = NULL;        

        // Dodaj do listy
        if (!devices) {
            devices = new_device;
        } else {
            d = devices;
            while (d->next) d = d->next;
            d->next = new_device;
        }

        notify_mqtt();
    }
}

void notify_mqtt(void) {
    cJSON *root = cJSON_CreateObject();
    if(!root) {
        fprintf(stderr, "out of memory!\n");
        return;   
    }
    cJSON *device_list = cJSON_CreateArray();
    if(!device_list) {
        fprintf(stderr, "out of memory!\n");
        cJSON_Delete(root);
        return;
    }

    DeviceInfo *d = devices;
    device_count = 0;
    while (d) {
        cJSON *device_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(device_obj, "mac", d->mac);
        cJSON_AddStringToObject(device_obj, "ip", d->ip);
        cJSON_AddStringToObject(device_obj, "hostName", d->hostName);
        cJSON_AddNumberToObject(device_obj, "switches", d->switches);
        cJSON_AddItemToArray(device_list, device_obj);

        device_count++;
        d = d->next;
    }
    cJSON_AddItemToObject(root, "devices", device_list);
    char *json_str = cJSON_Print(root);
    printf("Devices JSON:\n%s\n", json_str);
    printf("devices detected: %d\n", device_count);

    int rc = mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(json_str), json_str, 1, true);
    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "MQTT publish failed: %s\n", mosquitto_strerror(rc));
    }

    free(json_str);
    cJSON_Delete(root);
}

