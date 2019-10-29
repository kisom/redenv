#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <fcntl.h>
#include <strings.h>
#include "oled.h"

#include <Adafruit_SleepyDog.h>
#include <RTClib.h>
#include <SdFat.h>
/*#include <SFE_MicroOLED.h> */
#include <SparkFunBME280.h>
#include <SparkFunCCS811.h>
#include <Streaming.h>


#ifdef SAMD21
#define Serial		SerialUSB
#endif /* SAMD21 */
#define CCS811_ADDR	0x5B
#define MINUTE_MS	60000
#define OLED_DCJ	0
#define OLED_RESET	0
#define SD_CS		10


#define imageWidth 48
#define imageHeight 48


BME280		bme280;
CCS811		ccs811(CCS811_ADDR);
// MicroOLED	oled(OLED_RESET, OLED_DCJ);
OLED		oled(OLED_RESET, OLED_DCJ);
RTC_PCF8523	rtc;
SdFat		sd;


uint32_t	startupDTS;

/*
 * The BME280 can't compensate for the heat produced by the CCS811, so
 * it needs to be calibrated. These variables control this.
 *
 * TODO: figure out how to preserve these values through a soft (no
 * loss of power) reset.
 */
// uint32_t	ccs811Startup = 0;		// When was the ccs811 started?

/*
 * When the BME280 starts, take a reference temperature. This will be
 * used later to calibrate the temperature from the CCS811 starting up.
 * This presumes (probably wrongly) that the temperature will remain
 * fairly constant through the 20 minutes it takes for the CCS811 to power up.
 * This is only done once, at startup, so that sort of helps.
 */
float		coldStartTemperature = 0.0;
float		tempCal = 0.0;
bool		bme280Calibrated = false;


uint8_t			availableHardware = 0;
#define	HW_BME280	1
#define HW_CCS811	2
#define HW_RTC		4
#define HW_SD		8


typedef struct DTS {
	uint16_t	year;
	uint8_t		month;
	uint8_t		day;
	uint8_t		hour;
	uint8_t		minute;
	uint8_t		second;
} DTS;


DTS
now()
{
	DTS		dts;
	DateTime	when= rtc.now();

	dts.year = when.year();
	dts.month = when.month();
	dts.day = when.day();
	dts.hour = when.hour();
	dts.minute = when.minute();
	dts.second = when.second();

	return dts;
}



struct Reading {
	DTS		when;
	uint32_t	uptime;
	uint8_t		hw;	// available hardware
	float		temp;	// °C
	float		calt;	// temperature calibration value
	uint8_t		cal;	// is temperature calibrated?
	float		hum;	// % humidity
	float		press;	// kPa
	uint8_t		ccs811Status;
	int		co2;	// ppb
	int		tvoc;	// ppb
};


void
takeReading(struct Reading *r)
{
	r->when = now();
	r->uptime = rtc.now().unixtime() - startupDTS;
	r->hw = availableHardware;

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


void
printCCS811Status(uint8_t status)
{
	oled.clear(ALL);
	oled.clear(PAGE);
	oled.setCursor(0, 0);
	oled.print("CCS811 ERR");
	oled.setCursor(0, 8);
	Serial.print("CCS811 ERR: ");
	Serial.println(status);
	switch (status)
	{
		case CCS811Core::SENSOR_SUCCESS:
			oled.print("SUCCESS");
			break;
		case CCS811Core::SENSOR_ID_ERROR:
			oled.print("ID ERR");
			break;
		case CCS811Core::SENSOR_I2C_ERROR:
			oled.print("I2C ERR");
			break;
		case CCS811Core::SENSOR_INTERNAL_ERROR:
			oled.print("INT. ERR");
			break;
		case CCS811Core::SENSOR_GENERIC_ERROR:
			oled.print("GEN ERR");
			break;
		default:
			oled.print("UNK ERR");
	}
	oled.setCursor(0, 16);
	if (ccs811.appValid()) {
		oled.print("APP ERR");
		oled.setCursor(0, 24);
	}
	oled.display();
	delay(5000);
}


bool
setupBME280()
{
	bme280.settings.commInterface = I2C_MODE;
	bme280.settings.I2CAddress = 0x77;
	bme280.settings.runMode = 3; //  3, Normal mode
	bme280.settings.tStandby = 0; //  0, 0.5ms
	bme280.settings.filter = 0; //  0, filter off

	//tempOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.tempOverSample = 1;

	//pressOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.pressOverSample = 1;

	//humidOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.humidOverSample = 1;

	if (bme280.begin() != 0x60) {
		return false;
	}

	if (!bme280Calibrated) {
		coldStartTemperature = bme280.readTempC();
	}
	return true;
}


void
printReading(struct Reading *r)
{
	char	line1[11];
	char	line2[11];
	char	line3[11];
	char	line4[11];
	char	line5[11];
	char	line6[11];

	snprintf(line1, 11, "%02d%02d %02d:%02d",
			r->when.month, r->when.day,
			r->when.hour,  r->when.minute);
	snprintf(line2, 11, "%0.1fC %0.0f%%H",
			r->temp, r->hum);
	snprintf(line3, 11, "%0.1f KPA", (r->press / 1000));
	snprintf(line4, 11, "CO2: %d", r->co2);
	snprintf(line5, 11, "TVOC: %d", r->tvoc);
	snprintf(line6, 11, "HW: %d", r->hw);

	oled.clear(ALL);
	oled.clear(PAGE);
	oled.setCursor(0, 0);
	oled.print(line1);
	oled.setCursor(0, 8);
	oled.print(line2);
	oled.setCursor(0, 16);
	oled.print(line3);
	oled.setCursor(0, 24);
	oled.print(line4);
	oled.setCursor(0, 32);
	oled.print(line5);
	oled.setCursor(0, 40);
	oled.print(line6);
	oled.display();
}


void
serialReading(struct Reading *r)
{
	Serial << r->when.year;
	Serial << ",";
	Serial << r->when.month;
	Serial << ",";
	Serial << r->when.day;
	Serial << ",";
	Serial << r->when.hour;
	Serial << ",";
	Serial << r->when.minute;
	Serial << ",";
	Serial << r->when.second;
	Serial << ",";
	Serial << r->uptime;
	Serial << ",";
	Serial << r->hw;
	Serial << ",";
	Serial << r->temp;
	Serial << ",";
	Serial << r->calt;
	Serial << ",";
	Serial << r->cal;
	Serial << ",";
	Serial << r->hum;
	Serial << ",";
	Serial << r->press;
	Serial << ",";
	Serial << r->ccs811Status;
	Serial << ",";
	Serial << r->co2;
	Serial << ",";
	Serial << r->tvoc;
	Serial << "\n";
}


bool
writeReading(struct Reading *r)
{
	DateTime	now = rtc.now();
	char		path[17];

	if ((availableHardware & HW_SD) == 0) {
		return false;
	}

	snprintf(path, 16, "env_%04d%02d%02d.csv", now.year(), now.month(), now.day());
	ofstream logfile(path, O_WRONLY|O_APPEND);
	if (!logfile) {
		return false;
	}

	logfile << String(r->when.year);
	logfile << ",";
	logfile << String(r->when.month);
	logfile << ",";
	logfile << String(r->when.day);
	logfile << ",";
	logfile << String(r->when.hour);
	logfile << ",";
	logfile << String(r->when.minute);
	logfile << ",";
	logfile << String(r->when.second);
	logfile << ",";
	logfile << String(r->uptime);
	logfile << ",";
	logfile << String(r->hw);
	logfile << ",";
	logfile << String(r->temp);
	logfile << ",";
	logfile << String(r->calt);
	logfile << ",";
	logfile << String(r->cal);
	logfile << ",";
	logfile << String(r->hum);
	logfile << ",";
	logfile << String(r->press);
	logfile << ",";
	logfile << String(r->ccs811Status);
	logfile << ",";
	logfile << String(r->co2);
	logfile << ",";
	logfile << String(r->tvoc);
	logfile << "\n";
	logfile.close();
	return true;
}


void
setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH); // Show we're awake

	Serial.begin(9600);
	while (!Serial) ;
	Serial.println("SERIAL OK");

	Wire.begin();
	Wire.setClock(400000);
	Serial.println("I2C->400kHz");
	oled.begin();
	Serial.println("OLED STARTED");

	// clear(ALL) will clear out the OLED's graphic memory.
	// clear(PAGE) will clear the Arduino's display buffer.
	oled.clear(ALL);
	oled.clear(PAGE);
	oled.display();   

	// The RTC is pretty much required for this.
	if (!rtc.begin()) {
		oled.clear(ALL);
		oled.print("RTC FAIL");
		oled.display();

		Serial.println("RTC offline");
		while (1);
	}
	/*
	else {
		if (!rtc.initialized()) {
			oled.clear(ALL);
			oled.print("SETUP RTC");
			oled.display();

			Serial.println("RTC isn't initialized!");
			Serial.println("please run initialisation script!");
			while (1);
		}
	}
	*/

	if (!setupBME280()) {
		oled.clear(ALL);
		oled.print("BME280 FAIL");
		Serial.println("BME280 FAILED");
		delay(5000);
	}
	else {
		availableHardware |= HW_BME280;
		Serial.println("BME280 OK");
	}

	delay(100);
	auto ccs811Status = ccs811.begin();
	if (ccs811Status == CCS811Core::SENSOR_SUCCESS) {
		Serial.println("CCS811 OK");
		delay(100);
		ccs811.setDriveMode(0);
	}
	else {
		printCCS811Status((uint8_t)ccs811Status);
	}

	availableHardware |= HW_RTC;
	startupDTS = rtc.now().unixtime();

	if (sd.begin(SD_CS, SD_SCK_MHZ(50))) {
		availableHardware |= HW_SD;
		Serial.println("SD OK");
	}
	else {
		oled.clear(ALL);
		oled.print("SD FAIL");
		oled.display();
		delay(5000);
	}

	oled.setFontType(0);
	Serial.println("BOOT OK");
	oled.clear(ALL);
	oled.print("BOOT OK");
	oled.display();
	delay(5000);
}


int	wakeups = 0;


void
loop()
{
	Serial.print("WAKEUP #");
	Serial.println(wakeups++);
	struct Reading		r;

	Serial.println(rtc.now().unixtime() - startupDTS);
	if ((!bme280Calibrated) && ((rtc.now().unixtime() - startupDTS) >= 120)) {
		auto ccs811Status = ccs811.checkForStatusError();
		if (!ccs811Status) {
			ccs811.setDriveMode(2);
			availableHardware |= HW_CCS811;
			tempCal = bme280.readTempC();
			tempCal -= coldStartTemperature;
			bme280Calibrated = true;
			oled.clear(ALL);
			oled.print("CCS811 RDY");
			oled.display();
		}
		else {
			printCCS811Status(ccs811.getErrorRegister());
		}
	}

	takeReading(&r);
	if (!writeReading(&r)) {
		Serial.println("WR FAILED");
	}
	printReading(&r);
	serialReading(&r);

	// If the serial port is connected, we're drawing power via
	// the USB connector and don't need to worry about sleeping.
	// This helps with debugging.
	if (Serial) {
		for (uint8_t i = 0; i < 5; i++) {
			digitalWrite(LED_BUILTIN, LOW); // Show we're asleep
			delay(500);
			digitalWrite(LED_BUILTIN, HIGH);
			delay(500);
		}
		delay(MINUTE_MS - 5000);
	}
	else {
		digitalWrite(LED_BUILTIN, LOW); // Show we're asleep
		int sleepTime = MINUTE_MS;

		while (sleepTime > 0) {
			int sleepMS = Watchdog.sleep(sleepTime);
			sleepTime -= sleepMS;
		}
		digitalWrite(LED_BUILTIN, HIGH);

#if defined(USBCON) && !defined(USE_TINYUSB)
		USBDevice.attach();
#endif
	}
}
