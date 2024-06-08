#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MAX_JSON_SIZE 1024

unsigned long starttime;

Adafruit_BME280 bme; // I2C

char *status_ip = "";

WiFiClient espclient;
PubSubClient pubsubclient(espclient);

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
  Serial.println(F("wifi_connect() - Connecting to WiFi network..."));

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

  WiFi.forceSleepBegin();
  delay(1);
  WiFi.forceSleepWake();
  delay(1);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(SENSORID);

  // Force reconnect if invalid rtc data or counter is zero
  if (rtcValid && rtcData.counter != 0)
  {

    Serial.print("wifi_connect() - Channel is ");
    Serial.println(rtcData.channel, HEX);

    Serial.print("wifi_connect() - BSSID is ");
    Serial.print(rtcData.ap_mac[5], HEX);
    Serial.print(":");
    Serial.print(rtcData.ap_mac[4], HEX);
    Serial.print(":");
    Serial.print(rtcData.ap_mac[3], HEX);
    Serial.print(":");
    Serial.print(rtcData.ap_mac[2], HEX);
    Serial.print(":");
    Serial.print(rtcData.ap_mac[1], HEX);
    Serial.print(":");
    Serial.println(rtcData.ap_mac[0], HEX);

    Serial.print(F("wifi_connect() - Connecting the fast way..."));
    // The RTC data was good, make a quick connection

    WiFi.begin(WLAN_SSID, WLAN_PASSWORD, rtcData.channel, rtcData.ap_mac, true);
  }
  else
  {
    Serial.print(F("wifi_connect() - Connecting the regular way..."));
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
        Serial.print(F("wifi_connect() - Trying regular connect..."));
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
        Serial.println(F("wifi_connect() - Connection error, giving up and going to deepsleep."));
        deepsleep();
      }
    }
    if (waitcount == 200)
    {
      Serial.println();
      Serial.println(F("wifi_connect() - Connection error, giving up and going to deepsleep."));
      deepsleep();
    }
    // Sensor warmup
    bme.takeForcedMeasurement();
    delay(100);
  }

  Serial.println();

  IPAddress ip = WiFi.localIP();
  status_ip = new char[40]();
  sprintf(status_ip, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  Serial.print(F("wifi_connect() - Connected to WiFi network. Local IP: "));
  Serial.println(status_ip);

  const uint8_t *bssid = WiFi.BSSID();

  // Write current connection info back to RTC
  rtcData.channel = WiFi.channel();
  memcpy(rtcData.ap_mac, bssid, 6); // Copy 6 bytes of BSSID (AP's MAC address)

  Serial.print("wifi_connect() - Channel is ");
  Serial.println(rtcData.channel, HEX);

  Serial.print("wifi_connect() - BSSID is ");
  Serial.print(bssid[5], HEX);
  Serial.print(":");
  Serial.print(bssid[4], HEX);
  Serial.print(":");
  Serial.print(bssid[3], HEX);
  Serial.print(":");
  Serial.print(bssid[2], HEX);
  Serial.print(":");
  Serial.print(bssid[1], HEX);
  Serial.print(":");
  Serial.println(bssid[0], HEX);

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

  Serial.begin(9600);
  Serial.println(F("setup() - Starting..."));

  pinMode(A0, INPUT);
  // ESP.getResetReason()

  Wire.begin();

  unsigned status = bme.begin(0x76, &Wire);
  if (!status)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring, address, sensor ID!");
    Serial.print("SensorID was: 0x");
    Serial.println(bme.sensorID(), 16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    Serial.println("Giving up, going to deep sleep.");
    deepsleep();
  }

  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X2, // temperature
                  Adafruit_BME280::SAMPLING_X2, // pressure
                  Adafruit_BME280::SAMPLING_X2, // humidity
                  Adafruit_BME280::FILTER_X2);

  Serial.println("-- Default Test --");
  delay(100);

  wifi_connect();

  Serial.println(F("setup() - Initializing MQTT client"));
  pubsubclient.setBufferSize(MAX_JSON_SIZE);
  pubsubclient.setServer(MQTT_SERVER, MQTT_PORT);
}

void mqtt_reconnect()
{
  int waitcount = 0;
  while (!pubsubclient.connected())
  {
    Serial.print(F("mqtt_reconnect() - Attempting MQTT connection..."));
    // Attempt to connect
    if (!pubsubclient.connect(SENSORID, MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.print(F(" failed, rc="));
      Serial.print(pubsubclient.state());
      Serial.println(F(" try again..."));

      waitcount++;
      if (waitcount > 20)
      {
        Serial.println("Giving up, going to deep sleep.");
        deepsleep();
      }

      delay(100);
    }
    else
    {
      Serial.println(F(" ok"));
    }
  }
}

void loop()
{
  if (!pubsubclient.connected())
  {
    mqtt_reconnect();
  }

  pubsubclient.loop();

  bme.takeForcedMeasurement();

  float temp = bme.readTemperature();
  float pressure = bme.readPressure() / 100.0F;
  float humidity = bme.readHumidity();
  float vcc = (int)(analogRead(A0) / 1024.0 * VOLTAGE_FACTOR);

  float abshumidity = (6.112 * exp((17.67 * temp) / (temp + 243.5)) * humidity * 2.16741) / (273.15 + temp);

  Serial.print("Temperature = ");
  Serial.print(temp);
  Serial.println(" °C");

  Serial.print("Pressure = ");
  Serial.print(pressure);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Absolute humidity = ");
  Serial.print(abshumidity);
  Serial.println(" g/m3");

  Serial.print("Battery VCC = ");
  Serial.print(vcc);
  Serial.println(" mV");

  Serial.println();

  if (
      (temp >= -40.0 && temp <= 100.0)             // Temperature in valid range
      && (pressure >= 300.0 && pressure <= 1100.0) // Pressure in valid range
      && (humidity >= 0.0 && humidity <= 100.0)    // Humidity in valid range
  )
  {

    StaticJsonDocument<256> document;
    document["temperature"] = temp;
    document["pressure"] = pressure;
    document["humidity"] = humidity;
    document["abshumidity"] = abshumidity;
    document["vcc"] = vcc;
    document["localip"] = status_ip;
    document["wifichannel"] = rtcData.channel;
    document["deepsleepinseconds"] = DEELSPEEP_SECONDS;
    document["voltagefactor"] = VOLTAGE_FACTOR;
    document["cycletime"] = millis() - starttime;

    char buffer[MAX_JSON_SIZE];

    size_t buffersize = serializeJson(document, buffer);
    if (pubsubclient.connected())
    {
      pubsubclient.publish(MQTT_DATA_TOPIC, buffer);
    }
  }
  else
  {
    Serial.println("Ignoring invalid measurements!");
  }

  Serial.println("Going to deep sleep.");

  deepsleep();
}
