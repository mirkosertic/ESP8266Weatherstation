#ifndef SENSORDATA_H
#define SENSORDATA_H

const String CLEAR_NIGHT("clear-night");        // The sky is clear during the night.                       Icon = clear
const String CLOUDY("cloudy");                  // There are many clouds in the sky.                        Icon = cloudy
const String FOG("fog");                        // There is a thick mist or fog reducing visibility.        Icon = fog
const String HAIL("hail");                      // Hailstones are falling.                                  Icon = flurries
const String LIGHTING("lightning");             // Lightning/thunderstorms are occurring.                   Icon = tstorms
const String LIGHTING_RAINY("lightning-rainy"); // Lightning/thunderstorm is occurring along with rain.     Icon = chancetstorms
const String PARTLY_CLOUDY("partlycloudy");     // The sky is partially covered with clouds.                Icon = partlycloudy
const String POURING("pouring");                // It is raining heavily.                                   Icon = rain
const String RAINY("rainy");                    // It is raining.                                           Icon = chancerain
const String SNOWY("snowy");                    // It is snowing.                                           Icon = snow
const String SNOW_RAINY("snowy-rainy");         // It is snowing and raining at the same time.              Icon = sleet
const String SUNNY("sunny");                    // The sky is clear and the sun is shining.                 Icon = sunny
const String WINDY("windy");                    // It is windy.                                             Icon = hazy
const String WINDY_VARIANT("windy-variant");    // It is windy and cloudy.                                  Icon = cloudy
const String EXCEPTIONAL("exceptional");        //                                                          Icon = unknown

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

    String forecast;
    String latestUpdateTime;
};

#endif