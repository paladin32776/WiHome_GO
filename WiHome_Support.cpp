#include "WiHome_Support.h"

bool needMDNS=true;
bool needDNSandCWS=true;
EnoughTimePassed etp_Wifi(10000);
EnoughTimePassed etp_MQTT_retry(5000);
int MQTT_connect_count=0;

// Web server:
ConfigWebServer* cws;

// Telnet server
WiFiServer* telnetServer;
WiFiClient telnetClient;
bool telnetactive = false;

void telnethandle()
{
  if ((WiFi.status() == WL_CONNECTED) && (telnetactive==false))
  {
    telnetServer = new WiFiServer(23);
    telnetServer->begin();
    telnetServer->setNoDelay(true);
    telnetactive = true;
    Serial.printf("Telnet ready.\n");
  }
  else if ((WiFi.status() != WL_CONNECTED) && (telnetactive==true))
  {
    telnetServer->stop();
    delete telnetServer;
    telnetactive = false;
    Serial.printf("Telnet stopped.\n");
  }
  if (telnetactive)
  {
    if (telnetServer->hasClient())
    {
      if (!telnetClient)
      {
        telnetClient = telnetServer->available();
        delay(5);
        while (telnetClient.available()>0)
          telnetClient.read();
        tprintf("Debug via telnet:\n\n");
      }
    }
  }
}

void tprintf(char *fmt, ...)
{
    if (telnetactive && telnetClient)
    {
      char buf[100];
      va_list va;
      va_start (va, fmt);
      vsprintf (buf, fmt, va);
      telnetClient.print(buf);
      va_end (va);
    }
}

bool ConnectStation(char* ssid, char* passwd, char* mdns_client_name)
{
  if (WiFi.status()!=WL_CONNECTED || WiFi.getMode()!=WIFI_STA)
  {
    if (etp_Wifi.enough_time())
    {
      WiFi.softAPdisconnect(true);
      if (WiFi.isConnected())
        WiFi.disconnect();
      while (WiFi.status()==WL_CONNECTED)
        delay(50);
      Serial.printf("Connecting to %s\n",ssid);
      WiFi.begin(ssid,passwd);
      WiFi.hostname(mdns_client_name);
      needMDNS=true;
    }
  }
  if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA && needMDNS)
  {
    Serial.printf("Connected to station (IP=%s, name=%s).\nSetting up MDNS client:\n",
                  WiFi.localIP().toString().c_str(), WiFi.hostname().c_str());
    if (!MDNS.begin(mdns_client_name))
      Serial.println("Error setting up MDNS responder!");
    else
    {
      Serial.println("mDNS responder started");
      MDNS.addService("esp", "tcp", 8080); // Announce ßesp tcp service on port 8080
      needMDNS=false;
      ArduinoOTA.setPort(8266);
      ArduinoOTA.setHostname(mdns_client_name);
      ArduinoOTA.begin();
    }
  }
  if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA && !needMDNS)
  {
    ArduinoOTA.handle();
    telnethandle();
    return true;
  }
  return false;
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
bool MQTT_connect(Adafruit_MQTT_Client* mqtt)
{
  int8_t ret;
  // Stop if already connected.
  // if (!WiFi.isConnected())
  //   return false;
  if (mqtt->connected())
    return true;
  // If not connected and enough time has passed since last reconnection attempt
  if (etp_MQTT_retry.enough_time())
  {
    Serial.printf("Connecting to MQTT (attempt %d)... ", MQTT_connect_count);
    if ((ret = mqtt->connect()) != 0) // connect will return 0 for connected
    {
      Serial.println(mqtt->connectErrorString(ret));
      mqtt->disconnect();
      MQTT_connect_count++;
      if (MQTT_connect_count==MAX_MQTT_CONNECT_COUNT)
        ESP.restart();
      return false;
    }
    else
    {
      Serial.println("MQTT Connected!");
      MQTT_connect_count=0;
      return true;
    }
  }
  else
    return false;
}


void ConnectSoftAP(char* ssid, UserData* ud)
{
  if (WiFi.status()!=WL_DISCONNECTED || WiFi.getMode()!=WIFI_AP)
  {
    Serial.printf("Going to SoftAP mode:\n");
    WiFi.softAPdisconnect(true);
    if (WiFi.isConnected())
      WiFi.disconnect(true);
    while (WiFi.status()==WL_CONNECTED)
      delay(50);
    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    if (WiFi.softAP(ssid))
    {
      Serial.printf("Soft AP created!\n");
      Serial.printf("SoftAP IP: %s\n",WiFi.softAPIP().toString().c_str());
      Serial.printf("Status/Mode: %d/%d\n",WiFi.status(),WiFi.getMode());
    }
    else
    {
      Serial.printf("Soft AP creation FAILED.\n");
    }
  }
  else
  {
    if (needDNSandCWS)
    {
      cws = new ConfigWebServer(80,ud);
      needDNSandCWS=false;
    }
    // Handle webserver and dnsserver events
    cws->handleClient();
  }

}


ConfigWebServer::ConfigWebServer(int port, UserData* pud)
{
  userdata = pud;
  // Setup the DNS server redirecting all the domains to the apIP
  IPAddress apIP(192, 168, 4, 1);
  dnsServer = new DNSServer();
  dnsServer->start(DNS_PORT, "*", apIP);
  webserver = new ESP8266WebServer(port);
  webserver->on("/", std::bind(&ConfigWebServer::handleRoot, this));
  webserver->onNotFound(std::bind(&ConfigWebServer::handleRoot, this));
  webserver->on("/save_and_restart.php", std::bind(&ConfigWebServer::handleSaveAndRestart, this));
  webserver->begin();
  Serial.println("HTTP server started");
}


void ConfigWebServer::handleRoot()
{
  userdata->load();
  String html = html_config_form1;
  html += userdata->wlan_ssid;
  html += html_config_form2;
  html += userdata->wlan_pass;
  html += html_config_form3;
  html += userdata->mqtt_broker;
  html += html_config_form4;
  html += userdata->mdns_client_name;
  html += html_config_form5;
  webserver->send(200, "text/html", html);
}


void ConfigWebServer::handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webserver->uri();
  message += "\nMethod: ";
  message += (webserver->method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += webserver->args();
  message += "\n";
  for (uint8_t i=0; i<webserver->args(); i++)
  {
    message += " " + webserver->argName(i) + ": " + webserver->arg(i) + "\n";
  }
  webserver->send(404, "text/plain", message);
}


void ConfigWebServer::handleSaveAndRestart()
{
  char buf[32];
  String message = "Save and Restart\n\n";
  message += "URI: ";
  message += webserver->uri();
  message += "\nMethod: ";
  message += (webserver->method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += webserver->args();
  message += "\n";
  for (uint8_t i=0; i<webserver->args(); i++)
  {
    message += " " + webserver->argName(i) + ": " + webserver->arg(i) + "\n";
    if ((webserver->argName(i)).compareTo("wlan_ssid")==0)
      strcpy(userdata->wlan_ssid, (webserver->arg(i)).c_str());
    if ((webserver->argName(i)).compareTo("wlan_pass")==0)
      strcpy(userdata->wlan_pass, (webserver->arg(i)).c_str());
    if ((webserver->argName(i)).compareTo("mqtt_broker")==0)
      strcpy(userdata->mqtt_broker, (webserver->arg(i)).c_str());
    if ((webserver->argName(i)).compareTo("mdns_client_name")==0)
      strcpy(userdata->mdns_client_name, (webserver->arg(i)).c_str());
  }
  Serial.println("--- Data to be saved begin ---");
  Serial.println(userdata->wlan_ssid);
  Serial.println(userdata->wlan_pass);
  Serial.println(userdata->mqtt_broker);
  Serial.println(userdata->mdns_client_name);
  Serial.println("--- Data to be saved end ---");
  userdata->save();
  message += "Userdata saved to EEPROM.\n";
  Serial.println("Userdata saved to EEPROM.");
  webserver->send(200, "text/plain", message);
}


void ConfigWebServer::handleClient()
{
  dnsServer->processNextRequest();
  webserver->handleClient();
}
