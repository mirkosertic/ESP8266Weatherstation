#include <Arduino.h>

String LoggingFormatString(const char *format, ...);

#define LOGGING_ENABLED true

#ifdef LOGGING_ENABLED
#define INFO(msg) Serial.printf("[INFO] %ld %s:%s():%d - %s\n", millis(), __FILE__, __func__, __LINE__, msg)
#define INFO_VAR(msg, ...) Serial.printf("[INFO] %ld %s:%s():%d - %s\n", millis(), __FILE__, __func__, __LINE__, LoggingFormatString(msg, __VA_ARGS__).c_str())

#define WARN(msg) Serial.printf("[WARN] %ld %s:%s():%d - %s\n", millis(), __FILE__, __func__, __LINE__, msg)
#define WARN_VAR(msg, ...) Serial.printf("[WARN] %ld %s:%s():%d - %s\n", millis(), __FILE__, __func__, __LINE__, LoggingFormatString(msg, __VA_ARGS__).c_str())
#endif

#ifndef LOGGING_ENABLED
#define INFO(msg)
#define INFO_VAR(msg, ...)
#define WARN(msg)
#define WARN_VAR(msg, ...)
#endif