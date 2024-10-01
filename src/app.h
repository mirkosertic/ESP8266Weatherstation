#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <functional>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

class App
{
private:
    String name;
    String version;
    String manufacturer;
    String devicetype;
    WiFiClient *espclient;
    PubSubClient *pubsubclient;
    String mqttBrokerHost;
    String mqttBrokerUsername;
    String mqttBrokerPassword;
    int mqttBrokerPort;

    bool mqttinit;

public:
    App();

    virtual ~App();

    String computeUUID();

    String computeSerialNumber();

    void setName(String name);

    String getName();

    void setDeviceType(String devicetype);

    String computeTechnicalName();

    void setVersion(String version);

    void setManufacturer(String manufacturer);

    void setMQTTBrokerHost(String mqttBrokerHost);

    void setMQTTBrokerUsername(String mqttBrokerUsername);

    void setMQTTBrokerPassword(String mqttBrokerPassword);

    void setMQTTBrokerPort(int mqttBrokerPort);

    void MQTT_init();

    void MQTT_reconnect();

    void MQTT_announce_sensor(String notifyId, String title, String icon, String displayUnit, int decimalPrecision, String valueTemplate, String stateTopic, String deviceClass = "");

    void MQTT_publish(String topic, String payload);

    void loop();
};

#endif
