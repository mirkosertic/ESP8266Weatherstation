#include "homeassistant.h"

#include <ArduinoJson.h>
#include <time.h>

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
    if (this->httpClient.begin(this->wifiClient, String(HOMEASSISTANT_URL) + "/api/history/period?filter_entity_id=" + sensorId + "&minimal_response&no_attributes"))
    {
        this->httpClient.addHeader("Authorization", String("Bearer ") + HOMEASSISTANT_TOKEN);
        this->httpClient.addHeader("Content-Type", "application/json");
        int responsecode = this->httpClient.GET();

        if (responsecode == HTTP_CODE_OK)
        {
            INFO_VAR("Got response of %d bytes", this->httpClient.getSize());
            JsonDocument filter;
            JsonArray arr = filter.to<JsonArray>();
            JsonArray arr1 = arr.add<JsonArray>();
            JsonObject obj = arr1.add<JsonObject>();
            obj["entity_id"] = true;
            obj["state"] = true;
            JsonDocument json;
            DeserializationError error = deserializeJson(json, this->httpClient.getStream(), DeserializationOption::Filter(filter));

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

                        yield();
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
    this->httpClient.end();
}

SensorData Homeassistant::fetchSensorData()
{
    SensorData result;
    result.indoorTemperatureMin = 0;
    result.indoorTemperatureMax = 0;
    result.indoorHumidityMin = 0;
    result.indoorHumidityMax = 0;
    result.outdoorTemperature = 0;
    result.outdoorTemperatureMin = 0;
    result.outdoorTemperatureMax = 0;
    result.latestUpdateTime = "00:00";
    result.forecast = EXCEPTIONAL;

    SensorData *resultptr = &result;

    this->fillSensorData(this->sensorid_outdoor, [resultptr](float lastest, float min, float max)
                         {
        resultptr->outdoorTemperature = lastest;
        resultptr->outdoorTemperatureMin = min;
        resultptr->outdoorTemperatureMax = max; });

    yield();

    this->fillSensorData(this->sensorid_temperature, [resultptr](float lastest, float min, float max)
                         {
        resultptr->indoorTemperatureMin = min;
        resultptr->indoorTemperatureMax = max; });

    yield();

    this->fillSensorData(this->sensorid_humidity, [resultptr](float lastest, float min, float max)
                         {
        resultptr->indoorHumidityMin = min;
        resultptr->indoorHumidityMax = max; });

    yield();

    // Forecast
    INFO("Trying to fetch remote data");
    if (this->httpClient.begin(this->wifiClient, String(HOMEASSISTANT_URL) + "/api/states/weather.forecast_home"))
    {
        this->httpClient.addHeader("Authorization", String("Bearer ") + HOMEASSISTANT_TOKEN);
        this->httpClient.addHeader("Content-Type", "application/json");

        int responsecode = this->httpClient.GET();

        if (responsecode == HTTP_CODE_OK)
        {
            INFO_VAR("Got response of %d bytes", this->httpClient.getSize());
            JsonDocument filter;
            filter["state"] = true;
            JsonDocument json;
            DeserializationError error = deserializeJson(json, httpClient.getStream(), DeserializationOption::Filter(filter));

            if (error)
            {
                WARN_VAR("Failed to parse: %s", error.f_str());
            }
            else
            {
                JsonObject obj = json.as<JsonObject>();
                result.forecast = obj["state"].as<String>();
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
    this->httpClient.end();

    time_t now;
    tm tm;

    time(&now);
    localtime_r(&now, &tm);

    char buffer[20];
    sprintf(buffer, "%02d:%02d", tm.tm_hour, tm.tm_min);
    result.latestUpdateTime = buffer;

    return result;
}