#include <Arduino.h>
#include <RTClib.h>

RTC_PCF8523 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup () {

  while (!Serial) {
    delay(1);  // for Leonardo/Micro/Zero
  }

  Serial.begin(9600);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  // following line sets the RTC to the date & time this sketch was compiled
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

  Serial.println("READY");
  if (rtc.initialized()) {
    Serial.println("RTC OK");
  }
}

bool timeSet = false;

void
printTime(DateTime now)
{
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.print(" since midnight 1/1/1970 = ");
  Serial.println(now.unixtime());
  delay(1000);
}

uint8_t
readNumber()
{
  uint8_t n = Serial.read();
}

void loop () {
  if (Serial.available() >= 5) {
    uint8_t month, day, hour, minute, second;
    uint16_t year = 2019;
    
    month = Serial.read();
    day = Serial.read();
    hour = Serial.read();
    minute = Serial.read();
    second = Serial.read();

    Serial.println("received time:");
    DateTime now(year, month, day, hour, minute, second);
    printTime(now);
    rtc.adjust(now);
    timeSet = true;
  }

  if (timeSet) {
    printTime(rtc.now());
  }
}
