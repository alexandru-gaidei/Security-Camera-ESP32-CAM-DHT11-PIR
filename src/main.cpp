// to flash esp32-cam , GPIO 0 needs to be connected to GND so that you’re able to upload code

#include "Arduino.h"
#include "env.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "esp_http_server.h"
#include "DHT.h"
#include <ArduinoJson.h>


const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

String deviceId = DEVICE_ID;
String deviceName = DEVICE_NAME;

int led = 4;
int pir_sensor = 13;
int dht_sensor = 14;

#define DHTPIN dht_sensor
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

float temperature, humidity, heatindex;
bool pir;

unsigned long sensorPreviousMillis1 = 0;
const long sensorInterval1 = 5000; 

unsigned long sensorPreviousMillis2 = 0;
const long sensorInterval2 = 300; 


void processDHTSensorData();
void processPIRSensorData();


#define PART_BOUNDARY "123456789000000000000987654321"

#define CAMERA_MODEL_AI_THINKER

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t server_httpd = NULL;
httpd_handle_t stream_httpd = NULL;

StaticJsonDocument<200> doc;

static esp_err_t index_handler(httpd_req_t *req)
{
    doc.clear();

    JsonObject variables = doc.createNestedObject("variables");
    variables["temperature"] = temperature;
    variables["humidity"] = humidity;
    variables["heatindex"] = heatindex;
    variables["pir"] = pir;

    doc["id"] = DEVICE_ID;
    doc["name"] = DEVICE_NAME;

    String output;
    serializeJson(doc, output);

    const char* resp = output.c_str();
    httpd_resp_send(req, resp, -1);
    return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if(res != ESP_OK){
    return res;
  }

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    } else {
      if(fb->width > 400){
        if(fb->format != PIXFORMAT_JPEG){
          bool jpeg_converted = frame2jpg(fb, 90, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if(!jpeg_converted){
            Serial.println("JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if(res == ESP_OK){
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if(fb){
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if(_jpg_buf){
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if(res != ESP_OK){
      break;
    }
  }
  return res;
}

void startServer()
{
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
      .uri      = "/",
      .method   = HTTP_GET,
      .handler  = index_handler,
      .user_ctx = NULL
  };

  httpd_uri_t video_uri = {
    .uri       = "/video",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&server_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(server_httpd, &index_uri);
  }

  config.server_port += 1;
  config.ctrl_port += 1;
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &video_uri);
  }
}

                             
void setup()
{
  pinMode(led, OUTPUT);
  pinMode(pir_sensor, INPUT);
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 
  
  if(psramFound()) {
    config.frame_size = FRAMESIZE_XGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Wi-Fi connection
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  
  Serial.print("IP:");
  Serial.println(WiFi.localIP());
  
  dht.begin();

  startServer();
}

void loop()
{
  processDHTSensorData();
  processPIRSensorData();
}


void processPIRSensorData()
{
  unsigned long sensorCurrentMillis = millis();
  
  if (digitalRead(pir_sensor) == HIGH) {
    if(sensorCurrentMillis - sensorPreviousMillis2 >= sensorInterval2) {
      sensorPreviousMillis2 = sensorCurrentMillis;   
      digitalWrite(led, HIGH);
    }
    else {
      digitalWrite(led, LOW);
    }
    pir = true;
  }
  else {
    digitalWrite(led, LOW);
    pir = false;
  }
}

void processDHTSensorData()
{
  unsigned long sensorCurrentMillis = millis();
 
  if(sensorCurrentMillis - sensorPreviousMillis1 >= sensorInterval1) {
    sensorPreviousMillis1 = sensorCurrentMillis;   
 
    float ht  = dht.readHumidity();
    float tt  = dht.readTemperature();
    float hit = dht.computeHeatIndex(tt, ht, false);

    if (isnan(ht) || isnan(tt) || isnan(hit)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      humidity = ht;
      temperature = tt;
      heatindex = hit;
    }
  }
}
