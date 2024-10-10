#ifndef SENSORDATA_H
#define SENSORDATA_H

struct SensorData
{
    float indoorTemperature;
    float indoorTemperatureMin;
    float indoorTemperatureMax;

    float indoorHumidity;
    float indoorHumidityMin;
    float indoorHumidityMax;

    float outdoorTemperature;
    float outdoorTemperatureMin;
    float outdoorTemperatureMax;
};

#endif