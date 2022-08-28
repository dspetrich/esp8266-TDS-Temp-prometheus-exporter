# Prometheus ESP8266 TDS DS18B20 Exporter

[![GitHub release](https://img.shields.io/github/v/release/HON95/prometheus-esp8266-dht-exporter?label=Version)](https://github.com/HON95/prometheus-esp8266-dht-exporter/releases)

An IoT Prometheus exporter for measuring temperature and humidity, using an ESP8266 (Arduino-compatible) with a Wi-Fi module and a TDS sensor combined with an DS18B20 waterprove temperature sensor.

## Metrics

| Metric | Description | Unit |
| - | - | - |
| `iot_info` | Metadata about the device. | |
| `iot_water_temperture_celsius` | Water temperature. | `°C` |
| `_water_EC_value_mS` | Water EV value  temperature. | `mS` |
| `_water_TDS_value_PPT` | Water PPT value | `PPT` |

## Requirements

### Hardware

- ESP8266-based board (or some other appropriate Arduino-based board).
    - Tested with "Adafruit Feather HUZZAH ESP8266" and "WEMOS D1 Mini".
- TDS Sensor
- DS18B20 temperature sensor

### Software

- [Arduino IDE](https://www.arduino.cc/en/Main/Software)
    - Download and install.
- [esp8266 library for Arduino](https://github.com/esp8266/Arduino#installing-with-boards-manager)
    - See the instructions on the page.
- [DHT sensor library for ESPx](https://github.com/beegee-tokyo/DHTesp)
    - Install using the Arduino library manager.
    - You can also try the Adafruit one, but that one didn't work for me.

## Building

### Hardware

Using the "AZ Deliverz D1 Mini ESP8266Mod 12-F".

