#include <Adafruit_BME280.h>  // delete !
#include <Adafruit_Sensor.h>  // delete !
#include <DallasTemperature.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <Wire.h>

#include "config.h"
#include "version.h"

// Debug mode is enabled if not zero
#define DEBUG_MODE 1
enum LogLevel {
  DEBUG,
  INFO,
  ERROR,
};

// Board name
#define BOARD_NAME "ESP8266"
// Sensor name (should be the same)
#define SENSOR_NAME "TDS-Temp-Sensor"
// How long to cache the sensor results, in milliseconds
#define READ_SENSOR_INTERVAL 1000U
// How many times to try to read the sensor before returning an error
#define READ_TRY_COUNT 5

// EC-Sensor stuff
#define TdsSensorPin A0
#define VREF 5.0   // analog reference voltage(Volt) of the ADC
#define SCOUNT 30  // sum of sample point
#define TDS_CORRECTION_FACTOR 0.78F  // from calibration!
#define TDS_TO_EC_FACTOR 0.002F
#define READ_TDS_ANALOG_INTERVAL \
  40U  // every 40 milliseconds,read the analog value from the ADC

int analogBuffer[SCOUNT];  // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0., tdsValue = 0., ecValue = 0., temperature = 25.;

// Include DS18B20 stuff
// Der PIN D4 (GPIO 2) wird als BUS-Pin verwendet
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

// HTTP Server
ESP8266WebServer http_server(HTTP_SERVER_PORT);

void setup_sensors();
void setup_wifi();
void setup_http_server();
void handle_http_home_client();
void handle_http_metrics_client();
void read_sensors();
void log(char const *message, LogLevel level = LogLevel::INFO);

void setup(void) {
  char message[128];
  Serial.begin(SERIAL_PORT_NUM);

  setup_wifi();
  setup_http_server();
  snprintf(message, 128, "Prometheus namespace: %s", PROM_NAMESPACE);
  log(message);
  setup_sensors();
  log("Setup done");
}

void setup_sensors() {
  log("Setting up sensors");
  // Analog pin (TDS Sensor)
  pinMode(TdsSensorPin, INPUT);
  // DS18B20 initialisieren (Temparature sensor)
  DS18B20.begin();
  log("Sensors ready", LogLevel::DEBUG);
}

void setup_wifi() {
  char message[128];
  log("Setting up Wi-Fi");
  snprintf(message, 128, "Wi-Fi SSID: %s", WIFI_SSID);
  log(message, LogLevel::DEBUG);
  snprintf(message, 128, "MAC address: %s", WiFi.macAddress().c_str());
  log(message, LogLevel::DEBUG);
  snprintf(message, 128, "Initial hostname: %s", WiFi.hostname().c_str());
  log(message, LogLevel::DEBUG);

  WiFi.mode(WIFI_STA);

#if WIFI_IPV4_STATIC == true
  log("Using static IPv4 adressing");
  IPAddress static_address(WIFI_IPV4_ADDRESS);
  IPAddress static_subnet(WIFI_IPV4_SUBNET_MASK);
  IPAddress static_gateway(WIFI_IPV4_GATEWAY);
  IPAddress static_dns1(WIFI_IPV4_DNS_1);
  IPAddress static_dns2(WIFI_IPV4_DNS_2);
  if (!WiFi.config(static_address, static_gateway, static_subnet, static_dns1,
                   static_dns2)) {
    log("Failed to configure static addressing", LogLevel::ERROR);
  }
#endif

#ifdef WIFI_HOSTNAME
  log("Requesting hostname: " WIFI_HOSTNAME);
  if (WiFi.hostname(WIFI_HOSTNAME)) {
    log("Hostname changed");
  } else {
    log("Failed to change hostname (too long?)", LogLevel::ERROR);
  }
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    log("Wi-Fi connection not ready, waiting", LogLevel::DEBUG);
    delay(500);
  }

  log("Wi-Fi connected.");
  snprintf(message, 128, "SSID: %s", WiFi.SSID().c_str());
  log(message);
  snprintf(message, 128, "BSSID: %s", WiFi.BSSIDstr().c_str());
  log(message);
  snprintf(message, 128, "Hostname: %s", WiFi.hostname().c_str());
  log(message);
  snprintf(message, 128, "MAC address: %s", WiFi.macAddress().c_str());
  log(message);
  snprintf(message, 128, "IPv4 address: %s", WiFi.localIP().toString().c_str());
  log(message);
  snprintf(message, 128, "IPv4 subnet mask: %s",
           WiFi.subnetMask().toString().c_str());
  log(message);
  snprintf(message, 128, "IPv4 gateway: %s",
           WiFi.gatewayIP().toString().c_str());
  log(message);
  snprintf(message, 128, "Primary DNS server: %s",
           WiFi.dnsIP(0).toString().c_str());
  log(message);
  snprintf(message, 128, "Secondary DNS server: %s",
           WiFi.dnsIP(1).toString().c_str());
  log(message);
}
void setup_http_server() {
  char message[128];
  log("Setting up HTTP server");
  http_server.on("/", HTTPMethod::HTTP_GET, handle_http_root);
  http_server.on(HTTP_METRICS_ENDPOINT, HTTPMethod::HTTP_GET,
                 handle_http_metrics);
  http_server.onNotFound(handle_http_not_found);
  http_server.begin();
  log("HTTP server started", LogLevel::DEBUG);
  snprintf(message, 128, "Metrics endpoint: %s", HTTP_METRICS_ENDPOINT);
  log(message);
}

void loop(void) { http_server.handleClient(); }

void handle_http_root() {
  log_request();
  static size_t const BUFSIZE = 256;
  static char const *response_template =
      "Prometheus ESP8266 DHT Exporter by DSP.\n"
      "\n"
      "Usage: %s\n";
  char response[BUFSIZE];
  snprintf(response, BUFSIZE, response_template, HTTP_METRICS_ENDPOINT);
  http_server.send(200, "text/plain; charset=utf-8", response);
}

void handle_http_metrics() {
  log_request();
  static size_t const BUFSIZE = 1024;
  static char const *response_template =
      "# HELP " PROM_NAMESPACE
      "_info Metadata about the device.\n"
      "# TYPE " PROM_NAMESPACE
      "_info gauge\n"
      "# UNIT " PROM_NAMESPACE "_info \n" PROM_NAMESPACE
      "_info{version=\"%s\",board=\"%s\",sensor=\"%s\"} 1\n"
      "# HELP " PROM_NAMESPACE
      "_water_temperture_celsius Water temperature.\n"
      "# TYPE " PROM_NAMESPACE
      "_water_temperture_celsius gauge\n"
      "# UNIT " PROM_NAMESPACE
      "_water_temperture_celsius \u00B0C\n" PROM_NAMESPACE
      "_water_temperture_celsius %f\n"
      "# HELP " PROM_NAMESPACE
      "_water_EC_value_mS Water EC Value.\n"
      "# TYPE " PROM_NAMESPACE
      "_water_EC_value_mS gauge\n"
      "# UNIT " PROM_NAMESPACE "_water_EC_value_mS mS\n" PROM_NAMESPACE
      "_water_EC_value_mS %f\n"
      "# HELP " PROM_NAMESPACE
      "_water_TDS_value_PPT Water TDS Value\n"
      "# TYPE " PROM_NAMESPACE
      "_water_TDS_value_PPT gauge\n"
      "# UNIT " PROM_NAMESPACE "_water_TDS_value_PPT PPT\n" PROM_NAMESPACE
      "_water_TDS_value_PPT %f\n";

  read_sensors();
  if (isnan(temperature) || isnan(ecValue) || isnan(tdsValue)) {
    http_server.send(500, "text/plain; charset=utf-8", "Sensor error.");
    return;
  }

  char response[BUFSIZE];
  snprintf(response, BUFSIZE, response_template, VERSION, BOARD_NAME,
           SENSOR_NAME, temperature, ecValue, tdsValue);
  http_server.send(200, "text/plain; charset=utf-8", response);
}

void handle_http_not_found() {
  log_request();
  http_server.send(404, "text/plain; charset=utf-8", "Not found.");
}

void read_sensors() {
  char message[64];
  log("Read sensors...", LogLevel::DEBUG);
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > READ_TDS_ANALOG_INTERVAL) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(
        TdsSensorPin);  // read the analog value and store into the buffer
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) analogBufferIndex = 0;
  }
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > READ_SENSOR_INTERVAL) {
    printTimepoint = millis();

    // read and print temperature
    DS18B20.requestTemperatures();
    temperature = DS18B20.getTempCByIndex(0);
    snprintf(message, 64, "Temperature: %f °C", temperature);
    log(message, LogLevel::DEBUG);

    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    averageVoltage =
        getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF /
        1024.0;  // read the analog value more stable by the median filtering
                 // algorithm, and convert to voltage value
    float compensationCoefficient =
        1.0 +
        0.02 *
            (temperature -
             25.0);  // temperature compensation formula : fFinalResult(25 ^ C)
                     // = fFinalResult(current) / (1.0 + 0.02 * (fTP - 25.0));
    float compensationVolatge = averageVoltage / compensationCoefficient;
    // temperature compensation
    tdsValue = (133.42 * compensationVolatge * compensationVolatge *
                    compensationVolatge -
                255.86 * compensationVolatge * compensationVolatge +
                857.39 * compensationVolatge) *
               0.5 *
               TDS_CORRECTION_FACTOR;  // convert voltage value to tds value
    // Convert to EC value
    ecValue = tdsValue * TDS_TO_EC_FACTOR;
    snprintf(message, 64, "EC Value: %f mS", ecValue);
    log(message, LogLevel::DEBUG);

    // PPM (parts-per-million) to PPT (parts-per-thousands)
    tdsValue = tdsValue * 0.001;
    snprintf(message, 64, "TDS Value: %f PPT", tdsValue);
    log(message, LogLevel::DEBUG);
  }
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}

// //////////////////////////////////////////////////////////////
void log_request() {
  char message[128];
  char method_name[16];
  get_http_method_name(method_name, 16, http_server.method());
  snprintf(message, 128, "Request: client=%s:%u method=%s path=%s",
           http_server.client().remoteIP().toString().c_str(),
           http_server.client().remotePort(), method_name,
           http_server.uri().c_str());
  log(message, LogLevel::INFO);
}

void get_http_method_name(char *name, size_t name_length, HTTPMethod method) {
  switch (method) {
    case HTTP_GET:
      snprintf(name, name_length, "GET");
      break;
    case HTTP_HEAD:
      snprintf(name, name_length, "HEAD");
      break;
    case HTTP_POST:
      snprintf(name, name_length, "POST");
      break;
    case HTTP_PUT:
      snprintf(name, name_length, "PUT");
      break;
    case HTTP_PATCH:
      snprintf(name, name_length, "PATCH");
      break;
    case HTTP_DELETE:
      snprintf(name, name_length, "DELETE");
      break;
    case HTTP_OPTIONS:
      snprintf(name, name_length, "OPTIONS");
      break;
    default:
      snprintf(name, name_length, "UNKNOWN");
      break;
  }
}

void log(char const *message, LogLevel level) {
  if (DEBUG_MODE == 0 && level == LogLevel::DEBUG) {
    return;
  }
  // Will overflow after a while
  float seconds = millis() / 1000.0;
  char str_level[10];
  switch (level) {
    case DEBUG:
      strcpy(str_level, "DEBUG");
      break;
    case INFO:
      strcpy(str_level, "INFO");
      break;
    case ERROR:
      strcpy(str_level, "ERROR");
      break;
    default:
      break;
  }
  char record[150];
  snprintf(record, 150, "[%10.3f] [%-5s] %s", seconds, str_level, message);
  Serial.println(record);
}
