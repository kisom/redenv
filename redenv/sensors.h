#ifndef REDENV_SENSORS_H
#define REDENV_SENSORS_H


#include <RTClib.h>
#include <string.h>


static RTC_PCF8523	rtc;
static uint32_t     lastReadingTime = 0;

void	SOS();
bool	SensorInit();
void	RecordMeasurement(uint8_t *buf);
bool	GPSUpdate();
void	GPSWaitForFix();


#endif /* REDENV_SENSORS_H */
