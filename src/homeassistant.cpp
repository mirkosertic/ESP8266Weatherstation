#include "homeassistant.h"

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "logging.h"

Homeassistant::Homeassistant()
{
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

    INFO("Trying to fetch remote data");
    WiFiClient wifiClient;
    HTTPClient httpClient;
    if (httpClient.begin(wifiClient, "http://httpbin.org/ip"))
    {
        httpClient.addHeader("Authorization", "Bearer xyz");
        int responsecode = httpClient.GET();

        if (responsecode == HTTP_CODE_OK)
        {
            INFO("Got ok response");
            // TODO: Do something...
            String payload = httpClient.getString();
            JsonDocument json;
            DeserializationError error = deserializeJson(json, payload);
            if (error)
            {
                WARN_VAR("Failed to parse: %s with %s", payload, error.f_str());
            }
            else
            {
                INFO_VAR("What shall I do with the data? : %s", payload.c_str());
                // Do something here with the data
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

    return result;
}