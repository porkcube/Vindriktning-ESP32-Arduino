#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Unique device name for InfluxDB
#define DEVICE_NAME "Vindriktning001"
// 3 byte header + 16 data bytes + CS signal shows up as random 20th byte
#define MAX_BUFFER 32 // 20
// Max AQI value, in theory it shouldn't ever read above this as it's AQI's actual limit
#define MAX_AQI 500
// WiFi AP SSID
#define WIFI_SSID "DONTWANTNONEUNLESSYOUGOTBUNSHUN"
// WiFi password
#define WIFI_PASSWORD "thisisacleverpassword"
// InfluxDB server url, e.g. http://192.168.1.48:8086 (don't use localhost, always server name or ip address)
#define INFLUXDB_URL "http://10.0.0.92:8086"
// InfluxDB database name
#define INFLUXDB_DATABASE "telegraf"
// InfluxDB user
#define INFLUXDB_USER "influxdb"
// InfluxDB pass
#define INFLUXDB_PASSWORD "influxdb"
// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// #define TZ_INFO "EST+5EDT,M3.2.0/2,M11.1.0/2"
#define TZ_INFO "EST5EDT"
// use to help with noisy readings
int aqiOld;

// Single InfluxDB instance
InfluxDBClient client;

// Data point
Point sensor(DEVICE_NAME);

void setup() {
  Serial.begin(115200);    // USB
  Serial2.begin(9600);     // RX2
  // Serial.println("Waiting 5s...");
  // delay(5000);

  // configure client
  client.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DATABASE, INFLUXDB_USER, INFLUXDB_PASSWORD);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to: ");
  Serial.print(WIFI_SSID);
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  // Add tags
  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  byte DF[MAX_BUFFER];
  int byte = 0;
  int index = 0;
  bool debug = false; // set to true for useful console messages
  bool reading = false;

  // Store measured value into point
  sensor.clearFields();

  while (!reading) {
    if (Serial2.available() > 0) {

      if (index < MAX_BUFFER) {
        DF[index] = Serial2.read();

        if (debug) {
          Serial.print("DF[");
          Serial.print(index);
          Serial.print("] = ");
          Serial.println(DF[index], HEX);
        }

        if (index >= 6 && DF[index - 6] == 0x16 && DF[index - 5] == 0x11 && DF[index - 4] == 0x0B) {
          reading = true;
          break;
        }
      index++;
      }
    }

    if (index >= MAX_BUFFER) {
      if (debug) {
        Serial.print("MAX_BUFFER of ");
        Serial.print(MAX_BUFFER);
        Serial.println(" reached");
      }
      break;
    }
  }

  if (reading) {
    int aqi = (DF[index - 1] * 256) + DF[index];

    // If it reads > 500 or 0 something is way off
    if (aqi >= MAX_AQI || aqi <= 0) {
      aqi = aqiOld;
    } else {
      aqiOld = aqi;
    }

    if (debug) {
      Serial.print("aqi: ");
      Serial.print(aqi);
      Serial.print(" / aqiOld: ");
      Serial.println(aqiOld);
    }
    // Store AQI reading
    sensor.addField("aqi", aqi);
    reading = false;
  }

  // Report RSSI of currently connected network
  sensor.addField("rssi", WiFi.RSSI());

  // example of additional data
  // sensor.addField("foo", "bar");

  // Print what we are actually writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensor));
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Wait 20s
  Serial.println("Waiting 15s");
  delay(15000);
}
