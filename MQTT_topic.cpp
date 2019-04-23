#include "MQTT_topic.h"

MQTT_topic::MQTT_topic(char* part1, const char* part2)
{
  strcpy(topic,part1);
  strcat(topic,part2);
}
