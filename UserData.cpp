// Class to save and retrieve user data 
// for WiHome client
// Author: Gernot Fattinger (2018)

#include "UserData.h"

UserData::UserData()
{
}

bool UserData::load()
{
  EEPROM.begin(129);
  strcpy(wlan_ssid, "");
  strcpy(wlan_pass, "");
  strcpy(mqtt_broker, "");
  strcpy(mdns_client_name, "");
  EEPROM.get(EEPROM_UserData, ud_id);
  if (ud_id != EEPROM_ud_id)
    return false;
  EEPROM.get(EEPROM_UserData+1,wlan_ssid);
  EEPROM.get(EEPROM_UserData+33,wlan_pass);
  EEPROM.get(EEPROM_UserData+65,mqtt_broker);
  EEPROM.get(EEPROM_UserData+97,mdns_client_name);
  EEPROM.end();
  return true;
}

void UserData::save()
{
  EEPROM.begin(129);
  EEPROM.put(EEPROM_UserData+1,wlan_ssid);
  EEPROM.put(EEPROM_UserData+33,wlan_pass);
  EEPROM.put(EEPROM_UserData+65,mqtt_broker);
  EEPROM.put(EEPROM_UserData+97,mdns_client_name);
  ud_id = EEPROM_ud_id;
  EEPROM.write(EEPROM_UserData, ud_id);
  EEPROM.end();
  delay(100);
}

void UserData::show()
{
  if (ud_id==EEPROM_ud_id)
  {
    Serial.println("UserData -- BEGIN");
    Serial.print("wlan_ssid: ");
    Serial.println(wlan_ssid);
    Serial.print("wlan_pass: ");
    Serial.println(wlan_pass);
    Serial.print("mqtt_broker: ");
    Serial.println(mqtt_broker);
    Serial.print("mdns_client_name: ");
    Serial.println(mdns_client_name);
    Serial.println("UserData -- END");
  }
}
