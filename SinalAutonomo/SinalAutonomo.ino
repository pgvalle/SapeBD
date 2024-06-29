#include <SoftwareSerial.h>
#include <WiFiEsp.h>  // WifiEsp

#include <ArduinoJson.h>  // ArduinoJson

#include <ThreeWire.h>
#include <RtcDS1302.h>  // Rtc by Makuna

#include <TimeLib.h>  // Time by Michael Margolis
#include <TimeAlarms.h>  // TimeAlarms by Michael Margolis

#define RING_DURATION 5 // in seconds

const int relay  = 12,
          led    = 6,
          button = 8;

// Your WiFi credentials
static const char ssid[] = "Mefibosete24",
                  pass[] = "papito12345";

// Define the serial pins for the ESP-01 module
SoftwareSerial espSerial(10, 11);  // RX, TX

ThreeWire rtcWires(3, 4, 2);
RtcDS1302<ThreeWire> rtc(rtcWires);  // DAT, CLK, RST

AlarmID_t lastTurnOnAlarm = 0;

enum State
{
  OFF, ON, RINGING
} state = OFF;

void ring();
void turnOn();
void turnOff();

void setupPins();
void setupWiFiAndRTC();
void setupAlarms();

void setup()
{
  Serial.begin(115200);

  setupPins();
  setupWiFiAndRTC();
  setupAlarms();

  turnOn();
}

void blink()
{
  // set led according to state
  static unsigned long count = 0;
  static bool blinkState = HIGH;
  if (++count % 100 == 0)
  {
    blinkState = !blinkState;
  }
  digitalWrite(led, blinkState);
}

void loop()
{
  // Everything ok then just show on/off state
  if (rtc.IsDateTimeValid())
  {
    digitalWrite(led, state != OFF);
  }
  // there's something wrong so say it with led blink
  else
  {
    blink();
  }

  bool buttonPress = digitalRead(button);
  while(digitalRead(button))
  {
    buttonPress = true;
    Alarm.delay(5);
  }

  switch (state)
  {
    case RINGING:
    case ON:
      if (buttonPress)
      {
        turnOff();
      }
      break;
    case OFF:
      // Turned on manually
      if (buttonPress)
      {
        Alarm.free(lastTurnOnAlarm);
        lastTurnOnAlarm = 0;
        turnOn();
      }
      break;
  }

  Alarm.delay(5);
}

void printInformation()
{
  RtcDateTime now = rtc.GetDateTime();

  static char buffer[21];
  sprintf(buffer, "%02d:%02d:%02d, %02d/%02d/%4d",
          now.Hour(), now.Minute(), now.Second(),
          now.Day(), now.Month(), now.Year());

  Serial.print(buffer);
  Serial.print(" ");
  Serial.println(state);
}

void turnOn()
{
  digitalWrite(relay, HIGH);
  state = ON;

  printInformation();
}

void turnOff()
{
  // relay serves as ground and grounding the ssr activates the ringer
  digitalWrite(relay, HIGH);
  state = OFF;

  // Forget turnOn alarm set previously
  if (lastTurnOnAlarm)
  {
    Alarm.free(lastTurnOnAlarm);
  }
  // Stay off only for a day
  lastTurnOnAlarm = Alarm.timerOnce(24, 0, 0, turnOn);

  printInformation();
}

void ring()
{
  if (state == OFF || !rtc.IsDateTimeValid())
  {
    return;
  }

  // relay serves as ground and grounding the ssr activates the ringer
  digitalWrite(relay, LOW);
  state = RINGING;

  // Forget turnOn alarm set previously
  if (lastTurnOnAlarm)
  {
    Alarm.free(lastTurnOnAlarm);
  }
  // Stay off only for a day
  lastTurnOnAlarm = Alarm.timerOnce(0, 0, RING_DURATION, turnOn);

  printInformation();
}


void setupPins()
{
  // turn off builtin led
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // setup pins
  pinMode(led, OUTPUT);
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH);
}

void setupWiFiAndRTC()
{
  // Initialize the WiFiEsp library with the software serial
  espSerial.begin(9600);
  WiFi.init(&espSerial);
  WiFi.begin(ssid, pass);

  // setup rtc module
  rtc.Begin();
  rtc.SetIsRunning(true);
  rtc.SetIsWriteProtected(false);
}

void setupAlarms()
{
  // configure TimeLib with rtc
  setSyncInterval(2880);  // sync once each 2 days
  setSyncProvider([]() -> time_t
  {
    if (rtc.IsDateTimeValid())
    {
      return rtc.GetDateTime().Unix32Time();
    }

    const uint8_t status = WiFi.begin(ssid, pass);
    if (status != WL_CONNECTED)
    {
      return 0;
    }

    WiFiEspClient client;

    if (!client.connect("worldtimeapi.org", 80))
    {
      return 0;
    }

    client.println("GET /api/timezone/America/Sao_Paulo HTTP/1.1");
    client.println("Host: worldtimeapi.org");
    client.println("Connection: close");
    client.println();

    // ignore everything before the json string
    String response = client.readStringUntil('\n');
    while (!response.startsWith("{")) {
      response = client.readStringUntil('\n');
    }

    client.stop();

    JsonDocument doc;
    deserializeJson(doc, response);
    const time_t unixTime = doc["unixtime"];
  
    // sync with rtc
    RtcDateTime rtcDT(0);
    rtcDT.InitWithUnix32Time(unixTime - 10800);
    rtc.SetDateTime(rtcDT);
    return unixTime - 10800;
  });

  // incio ebd
  Alarm.alarmRepeat(dowSunday, 9, 0, 0, ring);
  // final ebd
  Alarm.alarmRepeat(dowSunday, 11, 0, 0, ring);
  Alarm.alarmRepeat(dowSunday, 11, 10, 0, ring);
  // culto a noite
  Alarm.alarmRepeat(dowSunday, 18, 30, 0, ring);
}
