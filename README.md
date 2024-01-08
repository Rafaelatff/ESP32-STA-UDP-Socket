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

## Client

# Bibliography

During development, this content help me a lot:

* [4 - ESP IDF 5.0 - UDP Socket Server on ESP32](https://www.youtube.com/watch?v=qZqX0epzXF0&t=307s).
* [5 - ESP IDF 5.0 - UDP Socket Client on ESP32](https://www.youtube.com/watch?v=WgUNzXCg3ek).
* [FreeRTOS-ESP-IDF-5.0-Socket](https://github.com/SIMS-IOT-Devices/FreeRTOS-ESP-IDF-5.0-Socket).




