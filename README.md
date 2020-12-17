<h1>ESP832-CAM DHT11/Led/Relay</h1>

## Overview

A simple example that implements API for ESP32-CAM WiFi chip, DHT11 sensor and PIR.  

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

`http://wifi-ip/video`

For video streaming