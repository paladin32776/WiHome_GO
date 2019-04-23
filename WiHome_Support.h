#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "WiHome_HTML.h"
#include <pgmspace.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "WiHome_Config.h"
#include "NoBounceButtons.h"
#include "UserData.h"
#include "EnoughTimePassed.h"
#include "SignalLED.h"

#define DEBUG_ESP_DNS
#define DEBUG_ESP_PORT Serial

#define MAX_SRV_CLIENTS 1

void telnethandle();
void tprintf(char *fmt, ...);

// Function to connect to WiFi and mDNS
bool ConnectStation(char* ssid, char* passwd, char* mdns_client_name);

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
bool MQTT_connect(Adafruit_MQTT_Client* mqtt);

// Function to create soft-AP
void ConnectSoftAP(char* ssid, UserData* ud);


// Web server class
class ConfigWebServer
{
  private:
    ESP8266WebServer* webserver;
    const byte DNS_PORT = 53;
    DNSServer* dnsServer;
    UserData* userdata;
  public:
     ConfigWebServer(int port, UserData* pud);
     void handleRoot();
     void handleNotFound();
     void handleSaveAndRestart();
     void handleClient();
};
