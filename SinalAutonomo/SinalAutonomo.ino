#include <ThreeWire.h>
#include <RtcDS1302.h>

#include <TimeLib.h>
#include <TimeAlarms.h>

/**
 * Uncomment the define here and upload the program to your arduino
 * to reconfigure time and date. Then, comment it back and reupload
 * the program. And don't forget to check the battery voltage!
 */
//#define CONFIGURE_TIME_DATE

#define BUTTON_PIN 13
#define RINGER_PIN 7
#define LED_PIN 3

#define RING_DURATION 5 // in seconds

ThreeWire myWire(9, 11, 5); // DAT, CLK, RST
RtcDS1302<ThreeWire> rtc(myWire);

enum State
{
  OFF, ON, RINGING
} state;

AlarmID_t lastTurnOnAlarm = 0;

void setup()
{
  Serial.begin(9600);

  // turn off builtin led
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  rtc.Begin();
  rtc.SetIsRunning(true);
  rtc.SetIsWriteProtected(false);

#ifdef CONFIGURE_TIME_DATE
  rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  printDateTime();
#else
  if (!rtc.IsDateTimeValid())
  {
    Serial.println("Replace battery and reconfigure time and date!");
  }

  RtcDateTime now = rtc.GetDateTime();
  setTime(now.Hour(), now.Minute(), now.Second(),
          now.Day(), now.Month(), now.Year());  

  Alarm.alarmRepeat(dowSunday, 11, 0, 0, ring);
  Alarm.alarmRepeat(dowSunday, 11, 10, 0, ring);

  // this pin serve as current to the switch
  pinMode(12, OUTPUT);
  digitalWrite(12, HIGH);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(RINGER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  turnOn();
#endif
}

void loop()
{
#ifdef CONFIGURE_TIME_DATE
  printDateTime();
  delay(1000);
#else
  // write led state according to time valid
  digitalWrite(LED_PIN, !rtc.IsDateTimeValid());

  bool buttonPress = digitalRead(BUTTON_PIN);
  while(digitalRead(BUTTON_PIN))
  {
    buttonPress = true;
    Alarm.delay(1);
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
  }

  Alarm.delay(1);
#endif
}

void printDateTime()
{
#ifndef CONFIGURE_TIME_DATE
  Serial.print("State: ");
  Serial.print(state);
  Serial.print(", ");
#endif

  RtcDateTime now = rtc.GetDateTime();

  static char buffer[21];
  sprintf(buffer, "%02d:%02d:%02d, %02d/%02d/%4d",
          now.Hour(), now.Minute(), now.Second(),
          now.Day(), now.Month(), now.Year());

  Serial.println(buffer);
}

void turnOn()
{
  digitalWrite(RINGER_PIN, HIGH);
  state = ON;

  printDateTime();
}

void turnOff()
{
  // RINGER_PIN serves as ground and grounding the ssr activates the ringer
  digitalWrite(RINGER_PIN, HIGH);
  state = OFF;

  // Forget turnOn alarm set previously
  if (lastTurnOnAlarm)
  {
    Alarm.free(lastTurnOnAlarm);
  }
  // Stay off only for a day
  lastTurnOnAlarm = Alarm.timerOnce(24, 0, 0, turnOn);

  printDateTime();
}

void ring()
{
  // Only makes sense to ring if state != off
  if (state == OFF)
  {
    return;
  }

  // RINGER_PIN serves as ground and grounding the ssr activates the ringer
  digitalWrite(RINGER_PIN, LOW);
  state = RINGING;

  // Forget turnOn alarm set previously
  if (lastTurnOnAlarm)
  {
    Alarm.free(lastTurnOnAlarm);
  }
  // Stay off only for a day
  lastTurnOnAlarm = Alarm.timerOnce(0, 0, RING_DURATION, turnOn);

  printDateTime();
}
