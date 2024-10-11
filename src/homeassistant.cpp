#include "homeassistant.h"

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "logging.h"

Homeassistant::Homeassistant(String sensorid_outdoor, String sensorid_temperature, String sensorid_humidity)
{
    this->sensorid_outdoor = sensorid_outdoor;
    this->sensorid_temperature = sensorid_temperature;
    this->sensorid_humidity = sensorid_humidity;
}

void Homeassistant::fillSensorData(String sensorId, SensorDataCallback callback)
{
    INFO("Trying to fetch remote data");
    WiFiClient wifiClient;
    HTTPClient httpClient;
    if (httpClient.begin(wifiClient, String(HOMEASSISTANT_URL) + "/api/history/period?filter_entity_id=" + sensorId + "&minimal_response&no_attributes"))
    {
        httpClient.addHeader("Authorization", String("Bearer ") + HOMEASSISTANT_TOKEN);
        httpClient.addHeader("Content-Type", "application/json");
        int responsecode = httpClient.GET();

        if (responsecode == HTTP_CODE_OK)
        {
            INFO_VAR("Got response of %d bytes", httpClient.getSize());
            JsonDocument filter;
            JsonArray arr = filter.to<JsonArray>();
            JsonArray arr1 = arr.add<JsonArray>();
            JsonObject obj = arr1.add<JsonObject>();
            obj["entity_id"] = true;
            obj["state"] = true;
            JsonDocument json;
            DeserializationError error = deserializeJson(json, httpClient.getStream(), DeserializationOption::Filter(filter));

            if (error)
            {
                WARN_VAR("Failed to parse: %s", error.f_str());
            }
            else
            {
                JsonArray entities = json.as<JsonArray>();
                for (JsonVariant variant : entities)
                {
                    float minstate = 1000.0;
                    float maxstate = -1000.0;
                    float latest = 0;
                    String detectedEntityId = "";
                    for (JsonVariant state : variant.as<JsonArray>())
                    {
                        JsonObject obj = state.as<JsonObject>();
                        const char *entity_id = obj["entity_id"];
                        if (entity_id)
                        {
                            String e = entity_id;
                            detectedEntityId = e;
                        }
                        String statevalue = obj["state"];

                        latest = statevalue.toFloat();
                        minstate = min(minstate, latest);
                        maxstate = max(maxstate, latest);
                    }

                    INFO_VAR("Got %s min %d and max %d and latest %d", detectedEntityId.c_str(), (int)minstate, (int)maxstate, (int)latest);
                    callback(latest, minstate, maxstate);
                }
            }
        }
        else
        {
            WARN_VAR("Fetching sensor data resulted in HTTP response code %d", responsecode);
        }
    }
    else
    {
        WARN("Failed to fetch data");
    }
    httpClient.end();
}

SensorData Homeassistant::fetchSensorData()
{
    SensorData result;
    result.indoorTemperatureMin = -11;
    result.indoorTemperatureMax = 22;
    result.indoorHumidityMin = -33;
    result.indoorHumidityMax = 44;
    result.outdoorTemperature = -1;
    result.outdoorTemperatureMin = -55;
    result.outdoorTemperatureMax = 66;

    SensorData *resultptr = &result;

    this->fillSensorData(this->sensorid_outdoor, [resultptr](float lastest, float min, float max)
                         {
        resultptr->outdoorTemperature = lastest;
        resultptr->outdoorTemperatureMin = min;
        resultptr->outdoorTemperatureMax = max; });

    this->fillSensorData(this->sensorid_temperature, [resultptr](float lastest, float min, float max)
                         {
        resultptr->indoorTemperatureMin = min;
        resultptr->indoorTemperatureMax = max; });

    this->fillSensorData(this->sensorid_humidity, [resultptr](float lastest, float min, float max)
                         {
        resultptr->indoorHumidityMin = min;
        resultptr->indoorHumidityMax = max; });

    return result;
}