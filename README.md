# ESP32-STA-UDP-Socket
Repositorie created to document my jorney during WiFi configuration. This code uses FreeRTOS and lwIP middelware.

The plan here was to configure a WiFi network as AP and STA at the same time, in order to generate an Access Point (AP) for connection and also work as Station (STA). A station is a device that connects to a wireless network and communicates with the access point.

Then, in order to simplify the communication, configure the transport layer as UDP (User Datagram Protocol) instead of TCP (Transmission Control Protocol). Once the UDP doesn't require confirmation that the packet arrived, the packets are send only once (no retransmition is needed, nor time to acknoledge the packets). The packets in this case, are the MoT (Management over Tunnelament), that will keep RSSI values, router direction, hops allowed, .. basic management functions.

![image](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/e1a310a2-3400-4e64-bb58-d253b500ac71) Fonte [subspace](https://subspace.com/resources/tune-tcp-udp-performance). 

Then, send the MoT packets in broadcast mode, so all ESP32 in the network (with good signal reception) are able to receive it. The ESP boards nearby will receive the packet and check if this packet is addressed for this node, save the RSSI and forward as desired.

Well, I started by configuring the ESP32 as STA and connect it to my home network, ir order to track the packets if needed. Let's go coding.

## Wifi Configuration

Similar to [APSTA](https://github.com/Rafaelatff/esp-ap-and-ap-sta), the WiFi was configured as STA and connected to my home network. It was added two lines in the function `static esp_err_t init_wifi(void)`:

```c
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
```
And added the `wifi_event_handler` function, to help informing that WiFi STA condition:

```c
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
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data; // added
        ESP_LOGI(TAG, "Disconnected due to: %d", event->reason); // added
        esp_wifi_connect();
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        break;
    default:
        break;
    }
}
```
Results:

![image](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/9d1b4797-018a-4ff7-bb9a-269bcf8f2981)

## Server 

For the server side, I defined the same port for all the ports, being 2000.

```c
#define BROADCAST_UDP_CLIENT_DEST_PORT 2000
```
Then I configured using this port:
```c
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
```
And in the main, I just called the task:

```c
void app_main(void)
{
    ESP_ERROR_CHECK(init_wifi());   
    xTaskCreate(udp_server_task, "udp_server", 4096, (void *)AF_INET, 5, NULL);
}
```

After flashing the code on the ESP board, it returned the following IP code (192.168.0.148):

![image](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/0a030817-793f-4232-a50f-2021dda99e12)

In order to test, I use the `ncat` command (for UDP protocol):

![erro-cmd-udp](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/4c464c7b-a4a8-45f3-b7ac-fd82f816f8c9)

To make it work, I had to install the [Nmap](https://nmap.org/download.html). After sending the command `ncat -u 192.168.0.148 2000` to the server again, it worked and also send me a echo:

![funcionou](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/8262d492-d5c8-4b07-9393-64ed8098397c)

Since I want to work with broadcast messages, I change the server IP code for the broadcast IP (192.168.0.255) and it worked:

![broadcast](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/a92d4f02-587c-4b5d-b032-b454aa53f4c0)

Now let's work on the client code!

## Client

The client must know the IP address that he wants to send the messages. By programming each board with the server code, I was able to know the IP address that my router addressed to them. To make code easy I alread prepared the defines with the IP addresses to test:

```c
#define BROADCAST_CLIENT_DEST_IP "192.168.0.255" // Class C Broadcast address

#define GREEN_CLIENT_DEST_IP "192.168.0.148"  // Server IP Address
#define BLUE_CLIENT_DEST_IP "192.168.0.216"  // Server IP Address
#define PINK_CLIENT_DEST_IP "192.168.0.184"  // Server IP Address
#define ORANGE_CLIENT_DEST_IP "192.168.1.102"  // Server IP Address
#define YELLOW_CLIENT_DEST_IP "192.168.1.100"  // Server IP Address
```
Then the cliend code (is already with the broadcast IP):
```c
static void udp_client_task(void *pvParameters)
{
    char rx_buffer[128];
    char host_ip[] = BROADCAST_CLIENT_DEST_IP;
    //char host_ip[] = GREEN_CLIENT_DEST_IP;
    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(BROADCAST_CLIENT_DEST_IP);
        //dest_addr.sin_addr.s_addr = inet_addr(GREEN_CLIENT_DEST_IP);
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
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}
```
In the app_main I added the calling for the task:

```c
xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
```
First I tested sending to each ESP32 IP address separately. Then I program all the boards to send the broadcast message and leaved the GREEN on with the server being monitored. The result is as follow:

![nomes coretos](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/aa89225b-fa9d-47dd-a13e-6e84cce53a33)

It worked!

## Changing the ESP32 boards from STA to APSTA

I started working only on the GREEN and BLUE boards. I started to have some difficulties and after checking with a coleague, he told me I should start working with mesh instead of this type of network. I will keep the codes in this repositorie in case I wish to return to this topology.

When I was trying to connect one ESP32 board to another, it only returned me the following:

![error](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/6c349c12-ae19-492b-9ed8-43c1ea327307)

![reason number 201](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/fad2dc5c-c23e-42e9-9f50-3142874d4500)

I guess I need time to improve the network configuration code (such as set the IPs for my network, since it doesn't have the DHCP server of the router anymore).

Well I will focus on what is priority for my studies and then return here someday.

# Bibliography

During development, this content help me a lot:

* [4 - ESP IDF 5.0 - UDP Socket Server on ESP32](https://www.youtube.com/watch?v=qZqX0epzXF0&t=307s).
* [5 - ESP IDF 5.0 - UDP Socket Client on ESP32](https://www.youtube.com/watch?v=WgUNzXCg3ek).
* [FreeRTOS-ESP-IDF-5.0-Socket](https://github.com/SIMS-IOT-Devices/FreeRTOS-ESP-IDF-5.0-Socket).




