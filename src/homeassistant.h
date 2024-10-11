#ifndef HOMEASSISTANT_H
#define HOMEASSISTANT_H

#include <Arduino.h>
#include <functional>

#include "sensordata.h"

typedef std::function<void(float lastest, float min, float max)> SensorDataCallback;

class Homeassistant
{
private:
    String sensorid_outdoor;
    String sensorid_temperature;
    String sensorid_humidity;

    void fillSensorData(String sensorId, SensorDataCallback callback);

public:
    Homeassistant(String sensorid_outdoor, String sensorid_temperature, String sensorid_humidity);

    SensorData fetchSensorData();
};

#endif