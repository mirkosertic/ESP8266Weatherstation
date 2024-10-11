#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>

#include "logging.h"
#include "pins.h"
#include "app.h"
#include "homeassistant.h"
#include "display.h"

unsigned long starttime;

Adafruit_BME280 bme; // I2C

String stateTopic;

Display *inkdisplay = new Display();

App *app = new App();

Homeassistant *ha = new Homeassistant(
    String("sensor.") + app->computeTechnicalName(OUTDOOR_DEVICENAME) + "_temperature_sens",
    String("sensor.") + app->computeTechnicalName(DEVICENAME) + "_temperature_sens",
    String("sensor.") + app->computeTechnicalName(DEVICENAME) + "_relhumidity_sens");

// credit: https://gitlab.com/diy_bloke/verydeepsleep/blob/master/VeryDeepSleep.ino
//
// We make a structure to store connection information
// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.

struct
{
  uint32_t crc32;    // 4 bytes
  uint8_t channel;   // 1 byte,   5 in total
  uint8_t ap_mac[6]; // 6 bytes, 11 in total
  uint8_t counter;   // 1 byte,  12 in total
} rtcData;

// the CRC routine
uint32_t calculateCRC32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xffffffff;
  while (length--)
  {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1)
    {
      bool bit = crc & 0x80000000;
      if (c & i)
      {
        bit = !bit;
      }

      crc <<= 1;
      if (bit)
      {
        crc ^= 0x04c11db7;
      }
    }
  }

  return crc;
}

void deepsleep()
{
  WiFi.disconnect(true);
  delay(1);
  Serial.flush();
  ESP.deepSleep(DEELSPEEP_SECONDS * 1000000L, WAKE_RF_DISABLED);
}

void wifi_connect()
{
  INFO("Connecting to WiFi network...");

  // Try to read WiFi settings from RTC memory
  bool rtcValid = false;
  if (ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData)))
  {
    // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
    uint32_t crc = calculateCRC32(((uint8_t *)&rtcData) + 4, sizeof(rtcData) - 4);
    if (crc == rtcData.crc32)
    {
      rtcValid = true;
    }
  }

  app->setDeviceType("ESP8266 Climate Sensor");
  app->setName(DEVICENAME);
  app->setManufacturer("Mirko Sertic");
  app->setVersion("v1.0");

  app->setMQTTBrokerHost(MQTT_SERVER);
  app->setMQTTBrokerUsername(MQTT_USERNAME);
  app->setMQTTBrokerPassword(MQTT_PASSWORD);
  app->setMQTTBrokerPort(MQTT_PORT);

  WiFi.forceSleepBegin();
  delay(1);
  WiFi.forceSleepWake();
  delay(1);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(app->computeTechnicalName().c_str());

  // Force reconnect if invalid rtc data or counter is zero
  if (rtcValid && rtcData.counter != 0)
  {

    INFO_VAR("Channel is %X", rtcData.channel);
    INFO_VAR("BSSID is %X:%X:%X:%X:%X:%X", rtcData.ap_mac[0], rtcData.ap_mac[1], rtcData.ap_mac[2], rtcData.ap_mac[3], rtcData.ap_mac[4], rtcData.ap_mac[5]);

    INFO("Connecting the fast way...");
    // The RTC data was good, make a quick connection

    WiFi.begin(WLAN_SSID, WLAN_PASSWORD, rtcData.channel, rtcData.ap_mac, true);
  }
  else
  {
    INFO("Connecting the regular way...");
    // The RTC data was not valid, so make a regular connection
    WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
  }

  int waitcount;

  waitcount = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(F("."));
    waitcount++;
    if (waitcount == 100)
    {
      if (rtcValid)
      {
        Serial.println();
        INFO("Trying regular connect...");
        WiFi.disconnect(true);
        delay(10);
        WiFi.forceSleepBegin();
        delay(10);
        WiFi.forceSleepWake();
        delay(10);
        WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
      }
      else
      {
        Serial.println();
        WARN("Connection error, giving up and going to deepsleep.");
        deepsleep();
      }
    }
    if (waitcount == 200)
    {
      Serial.println();
      WARN("Connection error, giving up and going to deepsleep.");
      deepsleep();
    }
    // Sensor warmup
    bme.takeForcedMeasurement();
    delay(100);
  }

  Serial.println();

  IPAddress ip = WiFi.localIP();

  INFO_VAR("Connected to WiFi network. Local IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  const uint8_t *bssid = WiFi.BSSID();

  // Write current connection info back to RTC
  rtcData.channel = WiFi.channel();
  memcpy(rtcData.ap_mac, bssid, 6); // Copy 6 bytes of BSSID (AP's MAC address)

  INFO_VAR("Channel is %X", rtcData.channel);

  INFO_VAR("BSSID is %X:%X:%X:%X:%X:%X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

  if (rtcValid)
  {
    rtcData.counter = (rtcData.counter + 1) % 5;
  }
  else
  {
    rtcData.counter = 1;
  }

  rtcData.crc32 = calculateCRC32(((uint8_t *)&rtcData) + 4, sizeof(rtcData) - 4);
  ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData));
}

void setup()
{
  starttime = millis();
  Serial.begin(115200);

  // INFO_VAR("Starting. Reset reason is %s", ESP.getResetReason().c_str());

  inkdisplay->init();

  pinMode(GPIO_VOLTAGE_ADC, INPUT);

  Wire.begin(D2, D1);

  unsigned status = bme.begin(0x76, &Wire);
  if (!status)
  {
    INFO("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    INFO_VAR("SensorID was: 0x%02X, status = %d", bme.sensorID(), status);
    INFO("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    INFO("   ID of 0x56-0x58 represents a BMP 280,\n");
    INFO("        ID of 0x60 represents a BME 280.\n");
    INFO("        ID of 0x61 represents a BME 680.\n");
    INFO("Giving up, going to deep sleep.");
    deepsleep();
  }

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X2, // temperature
                  Adafruit_BME280::SAMPLING_X2, // pressure
                  Adafruit_BME280::SAMPLING_X2, // humidity
                  Adafruit_BME280::FILTER_X2);

  INFO("-- Default Test --");
  delay(100);

  wifi_connect();

  app->MQTT_init();

  stateTopic = app->computeTechnicalName() + "/state";

  app->MQTT_announce_sensor("cycletime", "Sensor cycle time", "mdi:timelapse", "ms", -1, "{{value_json.cycletime}}", stateTopic);
  app->MQTT_announce_sensor("temperature", "Temperature", "mdi:temperature-celsius", "°C", 2, "{{value_json.temperature}}", stateTopic, "TEMPERATURE");
  app->MQTT_announce_sensor("vcc", "Battery status", "mdi:car-battery", "mV", -1, "{{value_json.vcc}}", stateTopic, "VOLTAGE");
  app->MQTT_announce_sensor("pressure", "Pressure", "", " hPa", 2, "{{value_json.pressure}}", stateTopic, "ATMOSPHERIC_PRESSURE");
  app->MQTT_announce_sensor("relhumidity", "Humidity", "mdi:water-percent", "%", 2, "{{value_json.humidity}}", stateTopic, "HUMIDITY");
  app->MQTT_announce_sensor("abshumidity", "Abs. Humidity", "mdi:weight-gram", "g/m³", 2, "{{value_json.abshumidity}}", stateTopic);
  app->MQTT_announce_sensor("wifiquality", "WiFi Quality", "mdi:wifi", "dBm", -1, "{{value_json.rssi}}", stateTopic, "SIGNAL_STRENGTH");
}

void loop()
{
  app->loop();

  INFO("Taking new Measurement");

  bme.takeForcedMeasurement();

  float temp = bme.readTemperature();
  float pressure = bme.readPressure() / 100.0F;
  float humidity = bme.readHumidity();
  float vcc = (int)(analogRead(GPIO_VOLTAGE_ADC) / 1024.0 * VOLTAGE_FACTOR);

  float abshumidity = (6.112 * exp((17.67 * temp) / (temp + 243.5)) * humidity * 2.16741) / (273.15 + temp);

  INFO_VAR("Temperature       = %f °C", temp);
  INFO_VAR("Pressure          = %f hPa", pressure);
  INFO_VAR("Humidity          = %f %%", humidity);
  INFO_VAR("Absolute humidity = %f g/m³", abshumidity);
  INFO_VAR("Battery VCC       = %f mV", vcc);

  if (
      (temp >= -40.0 && temp <= 100.0)             // Temperature in valid range
      && (pressure >= 300.0 && pressure <= 1100.0) // Pressure in valid range
      && (humidity >= 0.0 && humidity <= 100.0)    // Humidity in valid range
  )
  {

    JsonDocument document;
    document["temperature"] = temp;
    document["pressure"] = pressure;
    document["humidity"] = humidity;
    document["abshumidity"] = abshumidity;
    document["vcc"] = vcc;
    document["wifichannel"] = rtcData.channel;
    document["deepsleepinseconds"] = DEELSPEEP_SECONDS;
    document["voltagefactor"] = VOLTAGE_FACTOR;
    document["cycletime"] = millis() - starttime;
    document["rssi"] = WiFi.RSSI();

    String buffer;

    serializeJson(document, buffer);
    app->MQTT_publish(stateTopic, buffer);
  }
  else
  {
    WARN("Ignoring invalid measurements!");
  }

  SensorData sensordata = ha->fetchSensorData();
  sensordata.indoorHumidity = humidity;
  sensordata.indoorTemperature = temp;

  inkdisplay->renderData(sensordata);

  INFO("Going to deep sleep.");
  deepsleep();
}
