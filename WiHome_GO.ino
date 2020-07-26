#include "WiHomeComm.h"
#include "NoBounceButtons.h"
#include "SignalLED.h"
#include "WiHome_Config.h"
#include "GateOpenerStateMachine.h"
#include "EnoughTimePassed.h"

WiHomeComm whc;

SignalLED led(PIN_LED,SLED_BLINK_FAST_1,PIN_LED_ACTIVE_LOW);
// SignalLED relay(PIN_RELAY,SLED_OFF,PIN_RELAY_ACTIVE_LOW);
GateOpenerStateMachine go(GO_MOT_PIN_A, GO_MOT_PIN_B, GO_POS_PIN, GO_ISENS_PIN, GO_LED_PIN, GO_NVM_OFFSET);
NoBounceButtons nbb;
char button;
EnoughTimePassed etp_position(POSITION_FEEBACK_INTERVALL); // Timer for position feedback while motor is running


void setup()
{
  if (SERIAL_DEBUG)
    Serial.begin(115200);
  else
    Serial.end();
  Serial.println();
  delay(100);
  button = nbb.create(PIN_BUTTON);
  go.dump_flash(GO_NVM_OFFSET,32);
  go.set_max_imotor(0);
  go.dump_flash(GO_NVM_OFFSET,32);
  go.set_auto_close_time(0);
  go.dump_flash(GO_NVM_OFFSET,32);
}


void loop()
{
  DynamicJsonBuffer jsonBuffer;
  int last_go_state = go.get_state();

  // Handling routines for various libraries used:
  JsonObject& root = whc.check(&jsonBuffer);
  nbb.check();
  led.check();
  go.check();

  if (go.just_stopped() || (go.is_running() && etp_position.enough_time()))
    whc.sendJSON("cmd","info",
                 "state",go.get_state(),
                 "position",go.get_position(),
                 "position_percent",go.get_position_percent(),
                 "imotor", go.get_imotor());

  // Logic for LED status display:
  if (whc.status()==1)
    led.set(SLED_OFF);
  else if (whc.status()==2)
    led.set(SLED_BLINK_FAST_3);
  else if (whc.status()==3)
    led.set(SLED_BLINK_FAST_1);
  else if (whc.status()==4)
    led.set(SLED_BLINK_SLOW);
  else
    led.set(SLED_BLINK_FAST);

  // React to button actions:
  if (nbb.action(button)==1)
  {
    if (whc.softAPmode==true)
      whc.softAPmode=false;
    else
    {
      Serial.printf("Button action=1, ");
      go.cycle();
      Serial.printf("state is now %d.\n",go.get_state());
      whc.sendJSON("cmd","info","state",go.get_state());
    }
    nbb.reset(button);
  }
  if ((nbb.action(button)==2) && (go.get_state()==0))
  {
    if (go.learn_closed_position())
      Serial.println("Stored closed position.");
    else
      Serial.println("Closed position cleared.");
    whc.sendJSON("cmd","info","closed_position",go.get_closed_position());
    nbb.reset(button);
  }
  if (nbb.action(button)==3)
  {
    Serial.printf("Button action=3, attempting to go to SoftAP mode.\n");
    whc.softAPmode=true;
    nbb.reset(button);
  }
  if (nbb.action(button)==4)
  {
    if (go.learn_open_position())
      Serial.println("Stored open position.");
    else
      Serial.println("Open position cleared.");
    whc.sendJSON("cmd","info","open_position",go.get_open_position());
    nbb.reset(button);
  }

  // React to received JSON command objects:
  if (root!=JsonObject::invalid())
  {
    if (root.containsKey("cmd"))
    {
      // Set commands:
      if (root["cmd"]=="set")
      {
        if (root.containsKey("state"))
        {
          go.set_state((int)root["state"]);
          whc.sendJSON("cmd","info","state",go.get_state());
        }
        if (root.containsKey("max_imotor"))
        {
          go.set_max_imotor((int)root["max_imotor"]);
          whc.sendJSON("cmd","info","max_imotor",go.get_max_imotor());
        }
        if (root.containsKey("auto_close_time"))
        {
          go.set_auto_close_time((int)root["auto_close_time"]);
          whc.sendJSON("cmd","info","auto_close_time",go.get_auto_close_time());
        }
        if (root.containsKey("max_on_time"))
        {
          go.set_max_on_time((int)root["max_on_time"]);
          whc.sendJSON("cmd","info","max_on_time",go.get_max_on_time());
        }
        if (root.containsKey("open_position"))
        {
          go.set_open_position((int)root["open_position"]);
          whc.sendJSON("cmd","info","open_position",go.get_open_position());
        }
        if (root.containsKey("closed_position"))
        {
          go.set_closed_position((int)root["closed_position"]);
          whc.sendJSON("cmd","info","closed_position",go.get_closed_position());
        }
      }
      // Learn/clear open position:
      if (root["cmd"]=="learn_open_position")
      {
        go.learn_open_position();
        whc.sendJSON("cmd","info","open_position",go.get_open_position());
      }
      // Learn/clear closed position:
      if (root["cmd"]=="learn_closed_position")
      {
        go.learn_closed_position();
        whc.sendJSON("cmd","info","closed_position",go.get_closed_position());
      }
      // Get status:
      if (root["cmd"]=="get_status")
          whc.sendJSON("cmd","info",
                       "state",go.get_state(),
                       "position",go.get_position(),
                       "position_percent",go.get_position_percent(),
                       "imotor", go.get_imotor());
      // Get parameters:
      if (root["cmd"]=="get_parameters")
          whc.sendJSON("cmd","info",
                       "max_imotor",go.get_max_imotor(),
                       "auto_close_time",go.get_auto_close_time(),
                       "closed_position",go.get_closed_position(),
                       "open_position", go.get_open_position(),
                       "max_on_time",go.get_max_on_time());
      // Get signal:
      if (root["cmd"]=="get_signal")
          whc.sendJSON("cmd","info","signal",WiFi.RSSI());

    }
  }

}
