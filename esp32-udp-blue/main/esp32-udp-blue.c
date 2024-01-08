#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h" // replaces the #include "tcpip_adapter.h"
#include <string.h> // to use 'memset'
#include "lwip/sockets.h" // To use sockets UDP
#include "esp_log.h"

#define MAXIMUM_AP 20
// BLUE COM4

#define AP_SSID "ESP32_AP_BLUE"
#define AP_PASSWORD "0123" 
#define AP_MAX_CONN 5
#define AP_CHANNEL 0
#define STA_SSID "ESP32_AP_GREEN"
#define STA_PASSWORD "0123"

static const char *TAG = "UDP BLUE";

#define BROADCAST_CLIENT_DEST_IP "192.168.0.255" // Class C Broadcast address
#define BROADCAST_UDP_CLIENT_DEST_PORT 2000

#define GREEN_CLIENT_DEST_IP "192.168.0.148"  // Server IP Address
#define BLUE_CLIENT_DEST_IP "192.168.0.216"  // Server IP Address
#define PINK_CLIENT_DEST_IP "192.168.0.184"  // Server IP Address
#define ORANGE_CLIENT_DEST_IP "192.168.1.102"  // Server IP Address
#define YELLOW_CLIENT_DEST_IP "192.168.1.106"  // Server IP Address

#define BUFFER_SIZE 64

const char *payload = "UDP GREEN";

/**
 * Estrutura do pacote a ser transmitido e recebido. O pacote possui 52 bytes, sendo 4 bytes de cabeçalho de camada física (Phy), 4 bytes de cabeçalho
 * de camada de Controle de Acesso ao Meio (MAC), 4 bytes de cabeçalho de camada de Rede (Net), 4 bytes de cabeçalho de camada de Transporte (Transp) e
 * e 4 bytes de cabeçalho de camada de Aplicação (App). Os 36 bytes restantes são reservados para payload de AD e IO.
 */
typedef struct{  
  /* Cabeçalho da camada Física (Phy) */
  uint8_t PhyHdr[4];
  /* Cabeçalho da camada de Controle de Acesso ao Meio (MAC) */
  uint8_t MACHdr[4];
  /* Cabeçalho da camada de Rede (Net) */
  uint8_t NetHdr[4];
  /* Cabeçalho da camada de Transporte (Transp) */
  uint8_t TranspHdr[4];
  /* Bytes o payload da corrente */
  uint8_t Data[36]; // 18 bytes subida + 18 bytes descida
}packet;
/* Simulação simples: Uma unica rota, dados RSSI serão salvos:
Data[5] = RSSI B1 (Green -> Blue) "recebido em BLUE"
Data[6] = RSSI 12 (Blue -> Pink)
Data[7] = RSSI 23 (Pink -> Orange)
Data[8] = RSSI 34 (Orange -> Yellow)
Data[9] = RSSI 43 (Yellow -> Orange)
Data[10] = RSSI 32 (Orange -> Pink)
Data[11] = RSSI 21 (Pink -> Blue)
Data[12] = RSSI 1B (Blue -> Green)
*/

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}

static esp_err_t init_wifi(void){

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT(); // macro default
    esp_wifi_init(&wifi_init_config);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

 
    esp_wifi_set_mode(WIFI_MODE_APSTA); // Changed to AP STA mode
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    wifi_config_t sta_config = {
        .sta = {
            .ssid = STA_SSID,
            .password = AP_PASSWORD,
        },
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .password = AP_PASSWORD,
            .max_connection = AP_MAX_CONN,
            .channel = AP_CHANNEL,
            .ssid_hidden = 0, // not show_hidden
        },
    };
    esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);

    esp_wifi_start();
    esp_wifi_connect();

    printf("WiFi initialization completed.");
    return ESP_OK;
}

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[128];
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    while (1)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(BROADCAST_UDP_CLIENT_DEST_PORT);
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket bound, port %d", BROADCAST_UDP_CLIENT_DEST_PORT);

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);

        while (1)
        {
            ESP_LOGI(TAG, "Waiting for data");
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            if (len > 0)
            {
                // Get the sender's ip address as string
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
                rx_buffer[len] = 0; // Null-terminate
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);
                sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            }
            else
            {
                ESP_LOGI(TAG, "Did not received data");
            }
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = BROADCAST_CLIENT_DEST_IP;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(BROADCAST_CLIENT_DEST_IP);
        dest_addr.sin_family = AF_INET; 
        dest_addr.sin_port = htons(BROADCAST_UDP_CLIENT_DEST_PORT);
        addr_family = AF_INET; //
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        //int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            //vTaskDelete(NULL);
            break;
        }

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", host_ip, BROADCAST_UDP_CLIENT_DEST_PORT);

        while (1) {

            int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Message sent");

            struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
                if (strncmp(rx_buffer, "OK: ", 4) == 0) {
                    ESP_LOGI(TAG, "Received expected message, reconnecting");
                    break;
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK(init_wifi());
    
    xTaskCreate(udp_server_task, "udp_server", 4096, (void *)AF_INET, 5, NULL);
    xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);

    while(1){

        vTaskDelay(3000/ portTICK_PERIOD_MS);
    }
}
