#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <time.h>

#include "logging.h"
#include "pins.h"
#include "app.h"
#include "homeassistant.h"
#include "display.h"
#include "rtc.h"

unsigned long starttime;
long timeToDisplayInit;
long timeToWifiConnect;
long timeToWMQTTInit;

Adafruit_BME280 bme; // I2C

String stateTopic;

Display inkdisplay;

App app;

RTC rtc;

Homeassistant ha = Homeassistant(
    String("sensor.") + app.computeTechnicalName(OUTDOOR_DEVICENAME) + "_temperature_sens",
    String("sensor.") + app.computeTechnicalName(DEVICENAME) + "_temperature_sens",
    String("sensor.") + app.computeTechnicalName(DEVICENAME) + "_relhumidity_sens");

struct rst_info *rtc_info = system_get_rst_info();

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

  rtc.restore();

  app.setDeviceType("ESP8266 Climate Sensor");
  app.setName(DEVICENAME);
  app.setManufacturer("Mirko Sertic");
  app.setVersion("v1.0");

  app.setMQTTBrokerHost(MQTT_SERVER);
  app.setMQTTBrokerUsername(MQTT_USERNAME);
  app.setMQTTBrokerPassword(MQTT_PASSWORD);
  app.setMQTTBrokerPort(MQTT_PORT);

  WiFi.forceSleepBegin();
  delay(1);
  WiFi.forceSleepWake();
  delay(1);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(app.computeTechnicalName().c_str());

  // Try to reconnect with known BSSID and Channel
  if (!rtc.connectWithStoredBSSIDAndChannel())
  {
    rtc.connect();
  }

  int waitcount;

  waitcount = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(F("."));
    waitcount++;
    if (waitcount == 100)
    {
      if (rtc.isValid())
      {
        Serial.println();
        INFO("Trying regular connect...");
        WiFi.disconnect(true);
        delay(10);
        WiFi.forceSleepBegin();
        delay(10);
        WiFi.forceSleepWake();
        delay(10);
        rtc.connect();
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

  configTime(TZ, NTP_SERVER);
}

void setup()
{
  starttime = millis();
  Serial.begin(115200);

  INFO_VAR("Running on Arduino : %s", ARDUINO_ESP8266_RELEASE);
  INFO_VAR("Running on SDK     : %s", ESP.getSdkVersion());
  INFO_VAR("Running on Core    : %s", ESP.getCoreVersion().c_str());
  INFO_VAR("Chip id is         : %d", ESP.getChipId());
  INFO_VAR("CPU is running at  : %d Mhz", ESP.getCpuFreqMHz());

  if (HAS_DISPLAY)
  {
    inkdisplay.init(rtc_info->reason == REASON_DEEP_SLEEP_AWAKE);
  }

  timeToDisplayInit = millis() - starttime;

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
                  Adafruit_BME280::SAMPLING_X4, // temperature
                  Adafruit_BME280::SAMPLING_X4, // pressure
                  Adafruit_BME280::SAMPLING_X4, // humidity
                  Adafruit_BME280::FILTER_X4);

  wifi_connect();

  timeToWifiConnect = millis() - starttime;

  app.MQTT_init();

  stateTopic = app.computeTechnicalName() + "/state";

  app.MQTT_announce_sensor("cycletime", "Sensor cycle time", "mdi:timelapse", "ms", -1, "{{value_json.cycletime}}", stateTopic);
  app.MQTT_announce_sensor("temperature", "Temperature", "mdi:temperature-celsius", "°C", 2, "{{value_json.temperature}}", stateTopic, "TEMPERATURE");
  app.MQTT_announce_sensor("vcc", "Battery status", "mdi:car-battery", "mV", -1, "{{value_json.vcc}}", stateTopic, "VOLTAGE");
  app.MQTT_announce_sensor("pressure", "Pressure", "", "hPa", 2, "{{value_json.pressure}}", stateTopic, "ATMOSPHERIC_PRESSURE");
  app.MQTT_announce_sensor("relhumidity", "Humidity", "mdi:water-percent", "%", 2, "{{value_json.humidity}}", stateTopic, "HUMIDITY");
  app.MQTT_announce_sensor("abshumidity", "Abs. Humidity", "mdi:weight-gram", "g/m³", 2, "{{value_json.abshumidity}}", stateTopic);
  app.MQTT_announce_sensor("wifiquality", "WiFi Quality", "mdi:wifi", "dBm", -1, "{{value_json.rssi}}", stateTopic, "SIGNAL_STRENGTH");

  timeToWMQTTInit = millis() - starttime;
}

void loop()
{
  app.loop();

  INFO("Taking new Measurement");

  for (int i = 0; i < 10; i++)
  {
    bme.takeForcedMeasurement();
    delay(5);
  }

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
    document["wifichannel"] = WiFi.channel();
    document["deepsleepinseconds"] = DEELSPEEP_SECONDS;
    document["voltagefactor"] = VOLTAGE_FACTOR;
    document["cycletime"] = millis() - starttime;
    document["rssi"] = WiFi.RSSI();
    document["timeToDisplayInit"] = timeToDisplayInit;
    document["timeToWifiConnect"] = timeToWifiConnect;
    document["timeToWMQTTInit"] = timeToWMQTTInit;
    document["rstreason"] = rtc_info->reason;

    String buffer;

    serializeJson(document, buffer);
    app.MQTT_publish(stateTopic, buffer);
  }
  else
  {
    WARN("Ignoring invalid measurements!");
    INFO("Going to deep sleep.");
    deepsleep();
  }

  rtc.save();

  // This can take up to two seconds...
  if (HAS_DISPLAY)
  {
    SensorData sensordata = ha.fetchSensorData();
    sensordata.indoorHumidity = humidity;
    sensordata.indoorTemperature = temp;

    // Disable WiFi here so save power...
    INFO("Disconnecting from WiFi");
    WiFi.disconnect(true);
    delay(10);
    WiFi.forceSleepBegin();

    long startDisplay = millis();
    inkdisplay.renderData(sensordata);
    INFO_VAR("Display took %d ms", millis() - startDisplay);
  }

  INFO("Going to deep sleep.");
  deepsleep();
}
