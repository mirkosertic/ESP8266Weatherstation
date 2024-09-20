#include "logging.h"

String LoggingFormatString(const char *format, ...)
{
  char buf[2048]; // Buffer to hold the formatted string
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  return String(buf);
}