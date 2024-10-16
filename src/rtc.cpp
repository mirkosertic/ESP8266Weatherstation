#include "rtc.h"

#include <ESP8266WiFi.h>

#include "logging.h"

RTC::RTC()
{
}

// the CRC routine
uint32_t RTC::calculateCRC32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xffffffff;
    while (length--)
    {
        uint8_t c = *data++;
        for (uint32_t i = 0x80; i > 0; i >>= 1)
        {
            bool bit = crc & 0x80000000;
            if (c & i)
            {
                bit = !bit;
            }

            crc <<= 1;
            if (bit)
            {
                crc ^= 0x04c11db7;
            }
        }
    }

    return crc;
}

void RTC::restore()
{
    // Try to read WiFi settings from RTC memory
    this->valid = false;
    if (ESP.rtcUserMemoryRead(0, (uint32_t *)&this->data, sizeof(RTCData)))
    {
        // Calculate the CRC of what we just read from RTC memory, but skip the first 4 bytes as that's the checksum itself.
        uint32_t crc = calculateCRC32(((uint8_t *)&this->data) + 4, sizeof(RTCData) - 4);
        if (crc == this->data.crc32)
        {
            INFO("RTC data is valid");
            this->valid = true;
        }
        else
        {
            WARN("RTC data is invalid");
        }
    }
}

void RTC::save()
{
    const uint8_t *bssid = WiFi.BSSID();

    // Write current connection info back to RTC
    this->data.channel = WiFi.channel();
    memcpy(this->data.ap_mac, bssid, 6); // Copy 6 bytes of BSSID (AP's MAC address)

    INFO_VAR("Channel is %X", this->data.channel);

    INFO_VAR("BSSID is %X:%X:%X:%X:%X:%X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

    if (this->valid)
    {
        this->data.counter = (this->data.counter + 1) % 5;
    }
    else
    {
        this->data.counter = 1;
    }

    INFO("Writing data to RTC");
    this->data.crc32 = calculateCRC32(((uint8_t *)&this->data) + 4, sizeof(RTCData) - 4);
    ESP.rtcUserMemoryWrite(0, (uint32_t *)&this->data, sizeof(RTCData));
}

bool RTC::isValid()
{
    return this->valid;
}

bool RTC::connectWithStoredBSSIDAndChannel()
{
    if (this->valid && this->data.counter != 0)
    {

        INFO_VAR("Channel is %X", this->data.channel);
        INFO_VAR("BSSID is %X:%X:%X:%X:%X:%X", this->data.ap_mac[0], this->data.ap_mac[1], this->data.ap_mac[2], this->data.ap_mac[3], this->data.ap_mac[4], this->data.ap_mac[5]);

        INFO("Connecting the fast way...");
        // The RTC data was good, make a quick connection

        WiFi.begin(WLAN_SSID, WLAN_PASSWORD, this->data.channel, this->data.ap_mac, true);

        return true;
    }

    return false;
}

void RTC::connect()
{
    INFO("Connecting the regular way...");
    // The RTC data was not valid, so make a regular connection
    WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
}
