# Vindriktning-ESP32-Arduino
Arduino code for ESP32/ESP8266 to read an Ikea Vindriktning AQI monitor and store in InfluxDB

There are other integration projects out there for things such as Home Assistant, but I needed one for InfluxDB(v1) to visualize in Grafana.

Requires:
  - Ikea Vindriktning
  - ESP32/ESP8266 mcu, 1/2 size model is more ideal but a full size does fit
  - `ESP8266 Influxdb` Arduino Library
