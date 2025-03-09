#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

#include "sensordata.h"

class Display
{
private:
    void renderRightAligned(String text, int x, int y);

public:
    Display();

    void init(bool fromdeepsleep = false);

    void renderData(SensorData sensorData);
};

#endif