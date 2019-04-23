#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "WiHome_Support.h"
#include "WiHome_Config.h"
#include "MQTT_topic.h"
#include "GateOpenerStateMachine.h"

const char version[] = "v1.1";
const char compile_date[] = __DATE__ " " __TIME__ ;

// Setup Wifi and MQTT Clients
// Create an ESP8266 WiFiClient object to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client* mqtt;

// User data class:
UserData ud;

// Setup button debouncing object:
NoBounceButtons nbb;
// and global variables to hold button IDs:
int button1;

// Setup led and relay:
SignalLED led1(PIN_LED, SLED_BLINK_FAST_1, PIN_LED_ACTIVE_LOW);
GateOpenerStateMachine* go;
int position_percent_last=-1;

// Create objects for EnoughTimePassed class:
EnoughTimePassed etp_MQTT_KeepAlive(MQTT_KEEPALIVE);
EnoughTimePassed etp_Position_Feedback(POSITION_FEEBACK_INTERVALL);

// Pointers to Topics for MQTT publish and subscribe:
MQTT_topic* t_stat_relay_feed;
MQTT_topic* t_cmd_relay_feed;
MQTT_topic* t_stat_position_feed;
MQTT_topic* t_stat_autoclose_feed;
MQTT_topic* t_cmd_autoclose_feed;
MQTT_topic* t_stat_imax_feed;
MQTT_topic* t_cmd_imax_feed;
MQTT_topic* t_stat_imotor_feed;
MQTT_topic* t_stat_console_feed;

// Pointers for publishing and subscribe MQTT objects:
Adafruit_MQTT_Publish* stat_relay_feed;
Adafruit_MQTT_Subscribe* cmd_relay_feed;
Adafruit_MQTT_Publish* stat_position_feed;
Adafruit_MQTT_Publish* stat_autoclose_feed;
Adafruit_MQTT_Subscribe* cmd_autoclose_feed;
Adafruit_MQTT_Publish* stat_imax_feed;
Adafruit_MQTT_Subscribe* cmd_imax_feed;
Adafruit_MQTT_Publish* stat_imotor_feed;
Adafruit_MQTT_Publish* stat_console_feed;

// Global variable to indicate soft AP mode, wlan and mqtt status, and existence of feeds:
bool is_softAP = false;
bool wlan_ok = false;
bool mqtt_ok = false;
bool mqtt_feeds_exist = false;

void MQTT_create_feeds()
{
  if (!mqtt_feeds_exist)
  {
    // Setup MQTT topics for feeds:
    t_stat_relay_feed = new MQTT_topic(ud.mdns_client_name,"/stat/relay");
    t_cmd_relay_feed = new MQTT_topic(ud.mdns_client_name, "/cmd/relay");
    t_stat_position_feed = new MQTT_topic(ud.mdns_client_name,"/stat/position");
    t_stat_autoclose_feed = new MQTT_topic(ud.mdns_client_name,"/stat/autoclose");
    t_cmd_autoclose_feed = new MQTT_topic(ud.mdns_client_name,"/cmd/autoclose");
    t_stat_imax_feed = new MQTT_topic(ud.mdns_client_name,"/stat/imax");
    t_cmd_imax_feed = new MQTT_topic(ud.mdns_client_name,"/cmd/imax");
    t_stat_imotor_feed = new MQTT_topic(ud.mdns_client_name,"/stat/imotor");
    t_stat_console_feed = new MQTT_topic(ud.mdns_client_name,"/stat/console");
    // Setup MQTT client:
    // mqtt = new Adafruit_MQTT_Client(&client, ud.mqtt_broker, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);
    mqtt = new Adafruit_MQTT_Client(&client, ud.mqtt_broker, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);
    // Setup MQTT subscriptions and publications
    stat_relay_feed = new Adafruit_MQTT_Publish(mqtt, t_stat_relay_feed->topic);
    cmd_relay_feed = new Adafruit_MQTT_Subscribe(mqtt, t_cmd_relay_feed->topic);
    stat_position_feed = new Adafruit_MQTT_Publish(mqtt, t_stat_position_feed->topic);
    stat_autoclose_feed = new Adafruit_MQTT_Publish(mqtt, t_stat_autoclose_feed->topic);
    cmd_autoclose_feed = new Adafruit_MQTT_Subscribe(mqtt, t_cmd_autoclose_feed->topic);
    stat_imax_feed = new Adafruit_MQTT_Publish(mqtt, t_stat_imax_feed->topic);
    cmd_imax_feed = new Adafruit_MQTT_Subscribe(mqtt, t_cmd_imax_feed->topic);
    stat_imotor_feed = new Adafruit_MQTT_Publish(mqtt, t_stat_imotor_feed->topic);
    stat_console_feed = new Adafruit_MQTT_Publish(mqtt, t_stat_console_feed->topic);
    mqtt->subscribe(cmd_relay_feed);
    mqtt->subscribe(cmd_autoclose_feed);
    mqtt->subscribe(cmd_imax_feed);
    mqtt_feeds_exist = true;
  }
}


void MQTT_destroy_feeds()
{
  if (mqtt_feeds_exist)
  {
    mqtt->unsubscribe(cmd_relay_feed);
    mqtt->unsubscribe(cmd_autoclose_feed);
    delete stat_relay_feed;
    delete cmd_relay_feed;
    delete stat_position_feed;
    delete stat_autoclose_feed;
    delete cmd_autoclose_feed;
    delete stat_imax_feed;
    delete cmd_imax_feed;
    delete stat_imotor_feed;
    delete stat_console_feed;
    delete mqtt;
    delete t_stat_relay_feed;
    delete t_cmd_relay_feed;
    delete t_stat_position_feed;
    delete t_stat_autoclose_feed;
    delete t_cmd_autoclose_feed;
    delete t_stat_imax_feed;
    delete t_cmd_imax_feed;
    delete t_stat_imotor_feed;
    delete t_stat_console_feed;
    mqtt_feeds_exist = false;
  }
}


void publish_state()
{
  char msg[64];
  sprintf(msg,"{\"state\":%d,\"position\":%d,\"imotor\":%d}",go->get_state(),go->get_position_percent(),go->get_imotor());
  stat_relay_feed->publish(msg);
  tprintf("%s\n",msg);
}


void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.printf("WiHome Gate Opener %s (%s)\n===========\nAuthor:\nGernot\nFattinger\n", version, compile_date);
  // Configure buttons:
  button1 = nbb.create(PIN_BUTTON);
  // Load user data (ssid, password, mdsn name, mqtt broker):
  ud.load();
  ud.show();
  // Create MQTT feeds:
  MQTT_create_feeds();
  go = new GateOpenerStateMachine(GO_MOT_PIN_A, GO_MOT_PIN_B, GO_POS_PIN, GO_ISENS_PIN, GO_LED_PIN, GO_NVM_OFFSET);
  go->dump_flash(GO_NVM_OFFSET,32);
  // go->set_max_imotor(70);
  // go->dump_flash(GO_NVM_OFFSET,32);
  // go->set_auto_close_time(10);
  // go->dump_flash(GO_NVM_OFFSET,32);
}


void loop_normal()
{
  // Ensure that WiFi Station connection is alive:
  // (will make the first connection and automatically reconnect when disconnected)
  wlan_ok = ConnectStation(ud.wlan_ssid, ud.wlan_pass, ud.mdns_client_name);

  if (wlan_ok)
    MQTT_create_feeds();
  else
    MQTT_destroy_feeds();
  // Ensure the connection to the MQTT server is alive (also checks wlan connection)
  // (will make the first connection and automatically reconnect when disconnected)
  if (wlan_ok)
    mqtt_ok = MQTT_connect(mqtt);
  else
    mqtt_ok = false;

  if (!mqtt_ok)
    MQTT_destroy_feeds();

  if (mqtt_ok)
  {
    // Check subscriptions:
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt->readSubscription(10))) // Wait for 10ms for a subscription message
    {
      if (subscription == cmd_relay_feed)
      {
        String command = (char*)cmd_relay_feed->lastread;
        Serial.print(F("Received: "));
        Serial.println(command);
        if (command.compareTo("open")==0)
        {
          Serial.println(F("open"));
          tprintf("open\n");
          go->open();
          publish_state();
        }
        if (command.compareTo("close")==0)
        {
            Serial.println(F("close"));
            tprintf("close\n");
            go->close();
            publish_state();
        }
        if (command.compareTo("stop")==0)
        {
            Serial.println(F("stop"));
            tprintf("stop\n");
            go->stop();
            publish_state();
        }
        if (command.compareTo("toggle")==0)
        {
          Serial.println(F("cycle"));
          tprintf("cycle\n");
          go->cycle();
          publish_state();
        }
        if (command.compareTo("status")==0)
        {
          Serial.println(F("Sending status ..."));
          tprintf("status\n");
          publish_state();
          stat_position_feed->publish(go->get_position_percent());
        }
        if (command.compareTo("autoclose")==0)
        {
          Serial.print(F("Sending autoclose time: "));
          Serial.println(go->get_auto_close_time());
          tprintf("Autoclose time: %d\n",go->get_auto_close_time());
          stat_autoclose_feed->publish(int(go->get_auto_close_time()));
        }
        if (command.compareTo("imax")==0)
        {
          Serial.print(F("Sending max motor current: "));
          Serial.println(go->get_max_imotor());
          tprintf("Max motor current: %d\n",go->get_max_imotor());
          stat_imax_feed->publish(int(go->get_max_imotor()));
        }
        if (command.compareTo("clearclosed")==0)
        {
          Serial.print(F("Clearing closed position."));
          if (go->valid_closed_position())
            go->learn_closed_position();
        }
        if (command.compareTo("setclosed")==0)
        {
          Serial.print(F("Setting closed position."));
          if (!go->valid_closed_position())
            go->learn_closed_position();
        }
        if (command.compareTo("version")==0)
        {
          char buf[60];
          sprintf(buf,"Version: %s (%s)",version,compile_date);
          Serial.println(buf);
          stat_console_feed->publish(buf);
        }
        if (command.compareTo("signal")==0)
        {
          char buf[20];
          long rssi = WiFi.RSSI();
          sprintf(buf,"RSSI = %d dBm",rssi);
          stat_console_feed->publish(buf);
        }
      }
      else if (subscription == cmd_autoclose_feed)
      {
        String command = (char*)cmd_autoclose_feed->lastread;
        Serial.print(F("New autoclose time received: "));
        Serial.println(command.toInt());
        go->set_auto_close_time(command.toInt());
        stat_autoclose_feed->publish(int(go->get_auto_close_time()));
      }
      else if (subscription == cmd_imax_feed)
      {
        String command = (char*)cmd_imax_feed->lastread;
        Serial.print(F("New max motor current received: "));
        Serial.println(command.toInt());
        go->set_max_imotor(command.toInt());
        stat_imax_feed->publish(int(go->get_max_imotor()));
      }
    }
  }

  // Logic for LED status display:
  if (mqtt_ok)
  {
    led1.set(SLED_OFF);
  }
  else if (wlan_ok)
  {
    led1.set(SLED_BLINK_FAST_3);
  }
  else
  {
    led1.set(SLED_BLINK_FAST_1);
  }

  // LED and GO checks:
  led1.check();
  go->check();
  // Check buttons:
  nbb.check();

  if (go->get_state()!=0)
    Serial.printf("pos=%3d  i=%4d\n",go->get_position(),go->get_imotor());

  if (etp_Position_Feedback.enough_time() && go->get_position_percent()!=position_percent_last
      && go->valid_open_position() && go->valid_closed_position() && mqtt_feeds_exist)
  {
    publish_state();
    stat_position_feed->publish(go->get_position_percent());
    stat_imotor_feed->publish(go->get_imotor());
    position_percent_last = go->get_position_percent();
  }

  // Publish if required:
  if (nbb.action(button1)==1)
  {
    Serial.print(F("\nButton1 pressed ..."));
    go->cycle();
    Serial.printf("state = %d ",go->get_state());
    if (mqtt_feeds_exist)
      publish_state();
    nbb.reset(button1);
  }
  if ((nbb.action(button1)==2) && (go->get_state()==0))
  {
    if (go->learn_closed_position())
      Serial.println("Stored closed position.");
    else
      Serial.println("Closed position cleared.");
    nbb.reset(button1);
  }
  if (nbb.action(button1)==3)
  {
    Serial.println("Going into SoftAP mode ...");
    is_softAP = true;
    MQTT_destroy_feeds();
    led1.set(SLED_BLINK_SLOW); // Blink led slow in SoftAP mode
    nbb.reset(button1);
  }

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  if (etp_MQTT_KeepAlive.enough_time() && WiFi.isConnected() && mqtt_ok)
    if(! mqtt->ping())
    {
      mqtt->disconnect();
      mqtt_ok=false;
      MQTT_destroy_feeds();
    }
}


void loop_softAP()
{
  ConnectSoftAP("WiHome_Config_AP", &ud);

  // LED checks:
  led1.check();
  // Check buttons
  nbb.check();

  if (nbb.action(button1)==1)
  {
    Serial.println("Going back to Station mode ...");
    is_softAP = false;
    MQTT_create_feeds();
    led1.set(SLED_OFF); // Turn off led when going to station mode
    nbb.reset(button1);
  }
}


void loop()
{
  if (is_softAP)
    loop_softAP();
  else
    loop_normal();
}
