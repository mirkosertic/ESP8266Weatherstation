#ifndef HOMEASSISTANT_H
#define HOMEASSISTANT_H

#include "sensordata.h"

class Homeassistant
{
public:
    Homeassistant();

    SensorData fetchSensorData();
};

#endif