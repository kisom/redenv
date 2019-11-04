#include <Adafruit_GPS.h>
#include <SparkFunBME280.h>
#include <SparkFunCCS811.h>
#include "sensors.h"


#define BME280_ID	0x60	/* from datasheet */
#define CCS811_ADDR     0x5B	/* from datasheet */
#define CCS811_RESET	A4
#define PVPIN		A0	/* middle pin of the solar cell trimpot tap */
#define READING_SIZE	41	/* how big is the reading struct in bytes */



#ifndef GPS_MODE
#define GPS_MODE	PMTK_SET_NMEA_OUTPUT_RMCGGA
#endif // GPS_MODE

#ifndef GPS_UPDATE_FREQ
#define GPS_UPDATE_FREQ	PMTK_SET_NMEA_UPDATE_1HZ
#endif // GPS_UPDATE_FREQ

#ifndef GPS_BAUDRATE
#define GPS_BAUDRATE	9600
#endif // GPS_BAUDRATE


static BME280		bme280;
static CCS811		ccs811(CCS811_ADDR);
static Adafruit_GPS	gps(&Serial1);


static uint32_t		startupDTS;
static bool		gpsFix = false;


/*
   The BME280 can't compensate for the heat produced by the CCS811, so
   it needs to be calibrated. These variables control this.

   TODO: figure out how to preserve these values through a soft (no
   loss of power) reset.

   When the BME280 starts, take a reference temperature. This will be
   used later to calibrate the temperature from the CCS811 starting up.
   This presumes (probably wrongly) that the temperature will remain
   fairly constant through the 20 minutes it takes for the CCS811 to power up.
   This is only done once, at startup, so that sort of helps.
*/
static float	coldStartTemperature = 0.0;
static float	tempCal = 0.0;
static bool	bme280Calibrated = false;


/*
   availableHardware is used to indicate which subsystems are alive;
   this allows reporting of broken subsystems.
*/
static uint8_t		availableHardware = 0;
#define HW_BME280	1
#define HW_CCS811	2
#define HW_RTC		4
#define HW_SD		8
#define HW_GPS		16


static void
s()
{
	digitalWrite(LED_BUILTIN, HIGH);
	delay(100);
	digitalWrite(LED_BUILTIN, LOW);
	delay(100);
	digitalWrite(LED_BUILTIN, HIGH);
	delay(100);
	digitalWrite(LED_BUILTIN, LOW);
	delay(100);
	digitalWrite(LED_BUILTIN, HIGH);
	delay(100);
	digitalWrite(LED_BUILTIN, LOW);
	delay(100);
}


static void
o()
{
	digitalWrite(LED_BUILTIN, HIGH);
	delay(300);
	digitalWrite(LED_BUILTIN, LOW);
	delay(300);
	digitalWrite(LED_BUILTIN, HIGH);
	delay(300);
	digitalWrite(LED_BUILTIN, LOW);
	delay(300);
	digitalWrite(LED_BUILTIN, HIGH);
	delay(300);
	digitalWrite(LED_BUILTIN, LOW);
	delay(300);
}


void
SOS()
{
	while (true) {
		s();
		delay(300);
		o();
		delay(300);
		s();
		delay(500);
	}
}


static uint32_t
gpsUnixTime()
{
	return DateTime(gps.year+2000, gps.month, gps.day, gps.hour,
			gps.minute, gps.seconds).unixtime();
}


uint8_t
getVoltage()
{
	float	voltage_f;
	/*
	   First, take the average of 10 readings to account
	   for small-scale voltage fluctuations.
	   */
	int	voltage = analogRead(PVPIN);

	for (auto i = 0; i < 9; i++) {
		voltage += analogRead(PVPIN);
		delayMicroseconds(11);
	}
	voltage /= 10;
	voltage_f = static_cast<float>(voltage);

	// The trimpot is set halfway, which divides the voltage in half.
	// Then, the voltage is divided by 10 again to scale it to a
	// uint8_t. So, 8.4V should be a value of 84.
	voltage_f *= 3.7;
	return (uint8_t)voltage_f;
}


// This can be unpacked with the following Python struct format:
// 	'<HBBBBBBIffffiiBBB'
struct Reading {
	uint16_t        year;
	uint8_t         month;
	uint8_t         day;
	uint8_t         hour;
	uint8_t         minute;
	uint8_t         second;
	uint8_t         hw;     // available hardware
	uint32_t        uptime;
	float           temp;   // °C
	float           calt;   // temperature calibration value
	float           hum;    // % humidity
	float           press;  // Pa
	int             co2;    // ppb
	int             tvoc;   // ppb
	uint8_t         voltage;
	uint8_t         ccs811Status;
	uint8_t         cal;    // is temperature calibrated?
	uint8_t		fix;	// is GPS fixed?
	uint8_t		sats;	// number of satellites
};



static void
now(struct Reading *r)
{
	r->year = gps.year+2000;
	r->month = gps.month;
	r->day = gps.day;
	r->hour = gps.hour;
	r->minute = gps.minute;
	r->second = gps.seconds;
}


static inline void
packReading(struct Reading *r, uint8_t *buf)
{
	memcpy(buf, r, READING_SIZE);
}


static bool
bme280Init()
{
	// Setup the BME280 for I2C communications in normal mode,
	// no filter, with oversampling rates of 1.
	bme280.settings.commInterface = I2C_MODE;
	bme280.settings.I2CAddress = 0x77;
	bme280.settings.runMode = 3;
	bme280.settings.tStandby = 0;
	bme280.settings.filter = 0;
	bme280.settings.tempOverSample = 1;
	bme280.settings.pressOverSample = 1;
	bme280.settings.humidOverSample = 1;

	if (bme280.begin() != BME280_ID) {
		return false;
	}

	coldStartTemperature = bme280.readTempC();
	return true;
}


static void
printCCS811Status(uint8_t status)
{
	Serial.print("CCS811 ERR: ");
	Serial.println(status);
	switch (status)
	{
		case CCS811Core::SENSOR_SUCCESS:
			Serial.println("SUCCESS");
			break;
		case CCS811Core::SENSOR_ID_ERROR:
			Serial.println("ID ERR");
			break;
		case CCS811Core::SENSOR_I2C_ERROR:
			Serial.println("I2C ERR");
			break;
		case CCS811Core::SENSOR_INTERNAL_ERROR:
			Serial.println("INT. ERR");
			break;
		case CCS811Core::SENSOR_GENERIC_ERROR:
			Serial.println("GEN ERR");
			break;
		default:
			Serial.println("UNK ERR");
	}
	if (ccs811.appValid()) {
		Serial.println("APP ERR");
	}
}


bool
CheckCCS811()
{
	if (bme280Calibrated) {
		return true;
	}

	if ((gpsUnixTime() - startupDTS) < 1200) {
		return false;
	}

	auto ccs811Status = ccs811.checkForStatusError();
	if (!ccs811Status) {
		ccs811.setDriveMode(2);
		availableHardware |= HW_CCS811;
		tempCal = bme280.readTempC();
		tempCal -= coldStartTemperature;
		bme280Calibrated = true;
		Serial.println("CCS811 RDY");
		return true;
	}
	else {
		printCCS811Status(ccs811.getErrorRegister());
		return false;
	}
}


void
takeReading(struct Reading *r)
{
	now(r);
	lastReadingTime = gpsUnixTime();
	r->uptime = lastReadingTime - startupDTS;
	r->hw = availableHardware;
	r->voltage = getVoltage();
	r->fix = gps.fix ? 1 : 0;
	r->sats = gps.satellites;

	yield();

	if ((r->hw & HW_BME280) != 0) {
		r->temp = bme280.readTempC();
		r->cal = bme280Calibrated ? 1 : 0;
		r->calt = tempCal;
		if (r->cal) {
			r->temp -= r->calt;
		}
		r->press = bme280.readFloatPressure();
		r->hum = bme280.readFloatHumidity();
	}

	yield();

	if ((r->hw & HW_CCS811) != 0) {
		if ((r->hw & HW_BME280) != 0) {
			ccs811.setEnvironmentalData(r->hum, r->temp);
		}

		if (ccs811.dataAvailable()) {
			ccs811.readAlgorithmResults();
			r->co2 = ccs811.getCO2();
			r->tvoc = ccs811.getTVOC();
		}

		if (ccs811.checkForStatusError()) {
			r->ccs811Status = ccs811.getErrorRegister();
			r->co2 = -1;
			r->tvoc = -1;
		}
		else {
			r->ccs811Status = 0;
		}
	}
	else {
		r->co2 = -1;
		r->tvoc = -1;
		r->ccs811Status = 255;
	}
}


bool
GPSUpdate()
{
	gps.read();
	if (gps.newNMEAreceived()) {
		gps.parse(gps.lastNMEA());
	}
	return gps.fix;
}


void
GPSWaitForFix()
{
	if (!gps.fix) {
		Serial.println("GPS: no fix");
	}
	while (!GPSUpdate()) {
		if ((millis() % 1000) == 0) {
			Serial.print("SATS: ");
			Serial.println((int)gps.satellites);
		}
	};
	Serial.println("GPS: fix");
}


bool
SensorInit()
{
	gps.begin(GPS_BAUDRATE);
	gps.sendCommand(GPS_MODE);
	gps.sendCommand(GPS_UPDATE_FREQ);
#ifndef GPS_NO_ANTENNA_UPDATES
	gps.sendCommand(PGCMD_ANTENNA);
#endif // GPS_NO_ANTENNA_UPDATES
	GPSWaitForFix();

	startupDTS = gpsUnixTime();
	availableHardware |= HW_GPS;

	// Pull the CCS811 low while we do other things to give it a
	// good reset. It requires a minimum reset pulse length of 20μs
	// to reset; the I2C communications in the BME280 setup will
	// more than cover this.
	pinMode(CCS811_RESET, OUTPUT);
	digitalWrite(CCS811_RESET, LOW);

	if (!bme280Init()) {
		Serial.println(F("BME280: FAILED"));
		SOS();
	}
	availableHardware |= HW_BME280;

	// Let the CCS811 fly free!
	digitalWrite(CCS811_RESET, HIGH);
	auto ccs811Status = ccs811.begin();
	if (ccs811Status == CCS811Core::SENSOR_SUCCESS) {
		delay(100);
		ccs811.setDriveMode(0);
	}
	else {
		printCCS811Status((uint8_t)ccs811Status);
		SOS();
	}


	Serial.print("READING SIZE: ");
	Serial.println(sizeof(struct Reading), DEC);

	Serial.println("SENSORS OK");
	return true;
}


void
RecordMeasurement(uint8_t *buf)
{
	struct Reading	r;

	takeReading(&r);
	packReading(&r, buf);
}

