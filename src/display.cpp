#include "display.h"

#include <SPI.h>

#include "logging.h"
#include "icons.h"

#define ENABLE_GxEPD2_GFX 0

// ESP8266 CS(SS)=15,SCL(SCK)=14,SDA(MOSI)=13,BUSY=16,RES(RST)=5,DC=4

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <bitmaps/Bitmaps200x200.h> // 1.54" b/w

#include "GxEPD2_display_selection_new_style.h"
#include "GxEPD2_selection_check.h"

Display::Display()
{
}

void Display::init()
{
    INFO("Init");
    display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
    // display.init(0); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
    INFO("Init done");

    display.setRotation(1);
    display.refresh();
}

void Display::renderRightAligned(String text, int x, int y)
{
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

    display.setCursor(x - tbw - 1, y);
    display.print(text);
}

void Display::renderData(SensorData sensorData)
{
    display.setFont(&FreeMonoBold24pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setFullWindow();
    display.fillScreen(GxEPD_WHITE);

    this->renderRightAligned(String((int)sensorData.indoorTemperature), 105, 35);
    this->renderRightAligned(String((int)sensorData.indoorHumidity), 105, 90);
    this->renderRightAligned(String((int)sensorData.outdoorTemperature), 105, 145);

    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(110, 85);
    display.print("%");

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(113, 15);
    display.print("o");

    display.setCursor(113, 126);
    display.print("o");

    display.setCursor(135, 15);
    display.print("Min");
    display.setCursor(135, 35);
    display.print("Max");
    this->renderRightAligned(String((int)sensorData.indoorTemperatureMin), 199, 15);
    this->renderRightAligned(String((int)sensorData.indoorTemperatureMax), 199, 35);

    display.setCursor(135, 70);
    display.print("Min");
    display.setCursor(135, 90);
    display.print("Max");
    this->renderRightAligned(String((int)sensorData.indoorHumidityMin), 199, 70);
    this->renderRightAligned(String((int)sensorData.indoorHumidityMax), 199, 90);

    display.setCursor(135, 125);
    display.print("Min");
    display.setCursor(135, 145);
    display.print("Max");
    this->renderRightAligned(String((int)sensorData.outdoorTemperatureMin), 199, 125);
    this->renderRightAligned(String((int)sensorData.outdoorTemperatureMax), 199, 145);

    display.drawBitmap(0, 6, epd_bitmap_temperature, 32, 32, GxEPD_WHITE, GxEPD_BLACK);

    display.drawBitmap(0, 61, epd_bitmap_hygrometer, 32, 32, GxEPD_WHITE, GxEPD_BLACK);

    display.drawBitmap(0, 116, epd_bitmap_outdoor, 32, 32, GxEPD_WHITE, GxEPD_BLACK);

    INFO("Before display");
    display.display(false);
    INFO("After display");

    display.hibernate();
}