<h1>ESP32-CAM DHT11/PIR (Led/Relay/Other)</h1>

## Overview

A simple example that implements Video Streaming and API calls for ESP32-CAM WiFi chip, DHT11 sensor and PIR (or another sensor modules).

This example uses [PlatformIO](https://platformio.org/) and [VSCode](https://code.visualstudio.com/) IDE.

## Setup

1. Rename `env.h.example` to `env.h`
2. Upload

## Endpoints
`http://wifi-ip`
```
{ 
   "variables":{ 
      "temperature":25.00,
      "humidity":33.00,
      "heatindex":24.42,
      "pir":0
   },
   "id":"1",
   "name":"Hall",
}
```

`http://wifi-ip:81/video`

For video streaming