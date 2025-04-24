#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "certs.h"

// ---------------------- TFT SETUP ---------------------
#define TFT_CS     5
#define TFT_RST    4
#define TFT_DC     2
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#define DARKGREY 0x31C6
#define MAX_TEMP 50.0  // Max expected temperature for scaling

// ---------------------- DHT SENSOR ---------------------
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------------- WIFI + MQTT ---------------------
const char* ssid = "Galaxy M21 2021 Edition825A";
const char* password = "yuvi2000";
const char* mqtt_server = "a360nz7xihng0l-ats.iot.us-east-1.amazonaws.com";
const int mqtt_port = 8883;
const char* mqtt_topic = "enviroMon/data";

WiFiClientSecure net;
PubSubClient client(net);

void setupCerts() {
  net.setCACert(AWS_ROOT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
}

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println(" connected!");
}

void connectAWS() {
  while (!client.connected()) {
    Serial.print("Connecting to AWS IoT...");
    if (client.connect("EnviroMonClient")) {
      Serial.println(" connected!");
    } else {
      Serial.print(" failed, rc="); Serial.println(client.state());
      delay(2000);
    }
  }
}

// ---------------------- UI DRAWING ---------------------
float lastTemp = NAN;
float lastHum = NAN;

void drawHeader() {
  tft.fillRect(0, 0, 128, 25, DARKGREY);
  tft.setTextColor(ST77XX_WHITE);
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(10, 20);
  tft.print("EnviroMon");
}

void drawBarGraph(float temp, float hum) {
  // Clear graph area
  tft.fillRect(0, 26, 128, 160 - 26, ST77XX_BLACK);

  // Temperature bar
  int tempFill = map(temp, 0, MAX_TEMP, 0, 100);
  tft.setCursor(10, 50);
  tft.setTextColor(ST77XX_RED); tft.setTextSize(1);
  tft.print("Temp: "); tft.print(temp, 1); tft.print("C");
  tft.drawRect(10, 60, 100, 15, ST77XX_WHITE);
  tft.fillRect(11, 61, tempFill, 13, ST77XX_RED);

  // Humidity bar
  int humFill = map(hum, 0, 100, 0, 100);
  tft.setCursor(10, 90);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("Humidity: "); tft.print(hum, 1); tft.print("%");
  tft.drawRect(10, 100, 100, 15, ST77XX_WHITE);
  tft.fillRect(11, 101, humFill, 13, ST77XX_CYAN);
}

// ---------------------- TIMING ---------------------
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 60000; // 1 minute

// ---------------------- MAIN SETUP ---------------------
void setup() {
  Serial.begin(115200);
  dht.begin();
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  drawHeader();

  LittleFS.begin(true);
  setupCerts();
  connectWiFi();
  client.setServer(mqtt_server, mqtt_port);
  connectAWS();
}

// ---------------------- MAIN LOOP ---------------------
void loop() {
  if (!client.connected()) connectAWS();
  client.loop();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("DHT read failed");
    return;
  }

  // Update UI only if values change
  if (temp != lastTemp || hum != lastHum) {
    lastTemp = temp;
    lastHum = hum;
    drawBarGraph(temp, hum);
    Serial.println("UI updated.");
  }

  // Publish to MQTT every minute
  unsigned long now = millis();
  if (now - lastPublishTime > publishInterval) {
    lastPublishTime = now;

    String payload = "{";
    payload += "\"temperature\":";
    payload += String(temp, 1);
    payload += ",\"humidity\":";
    payload += String(hum, 1);
    payload += "}";

    client.publish(mqtt_topic, payload.c_str());
    Serial.println("MQTT published: " + payload);
  }

  delay(2000);  // Sensor polling interval
}
