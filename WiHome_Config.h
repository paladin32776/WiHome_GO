#include "Arduino.h"

// MQTT Server Settings
#define MQTT_SERVERPORT  1883                   // use 8883 for SSL
#define MQTT_USERNAME    ""
#define MQTT_KEY         ""
#define MQTT_KEEPALIVE   300000
#define MAX_MQTT_CONNECT_COUNT 10

// Pin config Gosund wall switch
#define PIN_LED     5
#define PIN_LED_ACTIVE_LOW false
#define PIN_BUTTON  0

// WiHome GateOpener config
#define GO_MOT_PIN_A 12
#define GO_MOT_PIN_B 13
#define GO_POS_PIN 0 // Analog0 of ADS1015
#define GO_ISENS_PIN 1 // Analog1 of ADS1015
#define GO_LED_PIN 4
#define GO_LED_ACTIVE_LOW false
#define GO_NVM_OFFSET 256

// Interval to publish position via MQTT
#define POSITION_FEEBACK_INTERVALL 500
