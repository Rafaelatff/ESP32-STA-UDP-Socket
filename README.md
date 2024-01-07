# ESP32-STA-UDP-Socket
Repositorie created to document my jorney during WiFi configuration. This code uses FreeRTOS and lwIP middelware.

The plan here was to configure a WiFi network as [APSTA](https://github.com/Rafaelatff/esp-ap-and-ap-sta), in order to generate an Access Point (AP) for connection and also work as Station (STA). A station is a device that connects to a wireless network and communicates with the access point.

Then, in order to simplify the communication, configure the transport layer as UDP (User Datagram Protocol) instead of TCP (Transmission Control Protocol). Once the UDP doesn't require confirmation that the packet arrived, the packets are send only once (no retransmition is needed, nor time to acknoledge the packets). The packets in this case, are the MoT (Management over Tunnelament), that will keep RSSI values, router direction, hops allowed, .. basic management functions.

![image](https://github.com/Rafaelatff/ESP32-STA-UDP-Socket/assets/58916022/e1a310a2-3400-4e64-bb58-d253b500ac71) Fonte [subspace](https://subspace.com/resources/tune-tcp-udp-performance). 

Then, send the MoT packets in broadcast mode, so all ESP32 in the network (with good signal reception) are able to receive it. The ESP boards nearby will receive the packet and check if this packet is addressed for this node, save the RSSI and forward as desired.

Well, I started by configuring the ESP32 as STA and connect it to my home network, ir order to track the packets if needed. Let's go coding.

##
