#include "app.h"

#include <ArduinoJson.h>

#include "logging.h"
#include "gitrevision.h"

App::App()
{
    this->espclient = new WiFiClient();
    this->pubsubclient = new PubSubClient(*this->espclient);
    this->mqttinit = false;
}

App::~App()
{
    delete this->pubsubclient;
    delete this->espclient;
}

String App::computeUUID()
{
    String mac = String(WiFi.macAddress()) + "";
    mac.replace(':', '_');
    return mac;
}

String App::computeSerialNumber()
{
    return WiFi.macAddress();
}

void App::setName(String name)
{
    this->name = name;
}

String App::getName()
{
    return this->name = name;
}

void App::setDeviceType(String devicetype)
{
    this->devicetype = devicetype;
}

String App::computeTechnicalName(String deviceName)
{
    String tn = "" + deviceName;
    tn.replace(' ', '_');
    tn.toLowerCase();
    return tn;
}

String App::computeTechnicalName()
{
    return this->computeTechnicalName(this->name);
}

void App::setVersion(String version)
{
    this->version = version;
}

void App::setManufacturer(String manufacturer)
{
    this->manufacturer = manufacturer;
}

void App::setMQTTBrokerHost(String mqttBrokerHost)
{
    this->mqttBrokerHost = mqttBrokerHost;
}

void App::setMQTTBrokerUsername(String mqttBrokerUsername)
{
    this->mqttBrokerUsername = mqttBrokerUsername;
}

void App::setMQTTBrokerPassword(String mqttBrokerPassword)
{
    this->mqttBrokerPassword = mqttBrokerPassword;
}

void App::setMQTTBrokerPort(int mqttBrokerPort)
{
    this->mqttBrokerPort = mqttBrokerPort;
}

void App::MQTT_init()
{
    INFO_VAR("Initializing MQTT client to %s:%ld", this->mqttBrokerHost.c_str(), this->mqttBrokerPort);
    this->pubsubclient->setBufferSize(1024);
    this->pubsubclient->setServer(this->mqttBrokerHost.c_str(), this->mqttBrokerPort);

    String technicalName = this->computeTechnicalName();

    this->mqttinit = true;

    this->MQTT_reconnect();
}

void App::MQTT_reconnect()
{
    int waitcount = 0;
    while (!this->pubsubclient->connected())
    {
        INFO_VAR("Attempting MQTT connection with user %s ...", this->mqttBrokerUsername.c_str());
        // Attempt to connect
        if (!this->pubsubclient->connect(this->computeTechnicalName().c_str(), this->mqttBrokerUsername.c_str(), this->mqttBrokerPassword.c_str()))
        {
            WARN_VAR("failed, rc=%d, try again...", this->pubsubclient->state());

            waitcount++;
            if (waitcount > 20)
            {
                WARN("Giving up, restarting.");
                ESP.restart();
            }

            delay(100);
        }
        else
        {
            INFO("ok");
        }
    }
}

void App::MQTT_announce_sensor(String notifyId, String title, String icon, String displayUnit, int decimalPrecision, String valueTemplate, String stateTopic, String deviceClass)
{
    String technicalName = this->computeTechnicalName();
    String objectId = technicalName + "_" + notifyId + "_sens";
    String discoveryTopic = "homeassistant/sensor/" + objectId + "/config";

    String topicPrefix = technicalName + "/" + notifyId;

    JsonDocument discoverypayload;
    discoverypayload["object_id"] = objectId;
    discoverypayload["unique_id"] = objectId;
    discoverypayload["state_topic"] = stateTopic;
    discoverypayload["value_template"] = valueTemplate;
    discoverypayload["name"] = title;

    if (deviceClass.length() > 0)
    {
        discoverypayload["device_class"] = deviceClass;
    }
    if (icon.length() > 0)
    {
        discoverypayload["icon"] = icon;
    }
    if (displayUnit.length() > 0)
    {
        discoverypayload["unit_of_measurement"] = displayUnit;
    }
    if (decimalPrecision >= 0)
    {
        discoverypayload["suggested_display_precision"] = decimalPrecision;
    }

    JsonObject device = discoverypayload["device"].to<JsonObject>();
    device["name"] = this->name;
    device["model"] = this->devicetype;
    device["manufacturer"] = this->manufacturer;
    device["model_id"] = this->version;
    device["sw_version"] = String(gitRevShort);
    device["serial_number"] = this->computeSerialNumber();

    JsonArray identifiers = device["identifiers"].to<JsonArray>();
    identifiers.add(this->computeUUID());

    String json;
    serializeJson(discoverypayload, json);

    this->pubsubclient->publish(discoveryTopic.c_str(), json.c_str());
}

void App::MQTT_publish(String topic, String payload)
{
    if (this->mqttinit)
    {
        this->pubsubclient->publish(topic.c_str(), payload.c_str());
    }
}

void App::loop()
{
    if (this->mqttinit)
    {
        if (!this->pubsubclient->connected())
        {
            this->MQTT_reconnect();
        }

        this->pubsubclient->loop();
    }
}