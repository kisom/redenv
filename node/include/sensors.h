#ifndef REDENV_SENSORS_H
#define REDENV_SENSORS_H


#include <RTClib.h>
#include <string.h>


static RTC_PCF8523	rtc;

void	SOS();
bool	SensorInit();
void	RecordMeasurement(uint8_t *buf);


#endif /* REDENV_SENSORS_H */
