#ifndef RTC_H
#define RTC_H

#include <Arduino.h>

// credit: https://gitlab.com/diy_bloke/verydeepsleep/blob/master/VeryDeepSleep.ino
//
// We make a structure to store connection information
// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The access methods read and write 4 bytes at a time,
// so the RTC data structure should be padded to a 4-byte multiple.

struct RTCData
{
    uint32_t crc32;    // 4 bytes
    uint8_t channel;   // 1 byte,   5 in total
    uint8_t ap_mac[6]; // 6 bytes, 11 in total
    uint8_t counter;   // 1 byte,  12 in total
};

class RTC
{
private:
    RTCData data;
    bool valid;

    uint32_t calculateCRC32(const uint8_t *data, size_t length);

public:
    RTC();

    void restore();

    bool isValid();

    bool connectWithStoredBSSIDAndChannel();

    void connect();

    void save();
};

#endif