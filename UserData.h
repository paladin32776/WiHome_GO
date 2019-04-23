// Class to save and retrieve user data 
// for WiHome client
// Author: Gernot Fattinger (2018)

#include "Arduino.h"
#include <EEPROM.h>

// EEPROM adress config:
#define EEPROM_ud_id 213
#define EEPROM_UserData 0

class UserData
{
  public:
    byte ud_id = 0;
    char wlan_ssid[32];
    char wlan_pass[32];
    char mqtt_broker[32];
    char mdns_client_name[32];
    UserData();
    bool load();
    void save();
    void show(); // shows credentials via serial 
};
