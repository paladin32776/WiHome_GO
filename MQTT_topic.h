#include "Arduino.h"

class MQTT_topic
{
  public:
    char topic[64];
    MQTT_topic(char* part1, const char* part2);
};
