/*******************************************************************************
   Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
   Copyright (c) 2018 Terry Moore, MCCI

   Permission is hereby granted, free of charge, to anyone
   obtaining a copy of this document and accompanying files,
   to do whatever they want with them without any restriction,
   including, but not limited to, copying, modification and redistribution.
   NO WARRANTY OF ANY KIND IS PROVIDED.

   This example sends a valid LoRaWAN packet with payload "Hello,
   world!", using frequency and encryption settings matching those of
   the The Things Network. It's pre-configured for the Adafruit
   Feather M0 LoRa.

   This uses OTAA (Over-the-air activation), where where a DevEUI and
   application key is configured, which are used in an over-the-air
   activation procedure where a DevAddr and session keys are
   assigned/generated for use with all further communication.

   Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
   g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
   violated by this sketch when left running for longer)!

   To use this sketch, first register your application and device with
   the things network, to set or generate an AppEUI, DevEUI and AppKey.
   Multiple devices can use the same AppEUI, but each device has its own
   DevEUI and AppKey.

   Do not forget to define the radio type correctly in
   arduino-lmic/project_config/lmic_project_config.h or from your BOARDS.txt.

 *******************************************************************************/

#include <SPI.h>
#include <Wire.h>

#include <lmic.h>
#include <hal/hal.h>

#include "ttn.h"
#include "sensors.h"


#define CFG_us915 1
#define CFG_sx1276_radio 1

#define RFM95_CS	6
#define RFM95_IO1	11
#define RFM95_IRQ	5
#define RFM95_RST	9


// This is supplied for reference, and can't be used.
#ifndef REDENV_TTN_CONFIG
#error No TTN configuration supplied (missing ttn.h).
//
// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0xD5, 0xB3, 0x70
};


// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from the TTN console can be copied as-is.
static const u1_t PROGMEM APPKEY[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif /* REDENV_TTN_CONFIG */


void os_getArtEui (u1_t* buf) {
  memcpy_P(buf, APPEUI, 8);
}
void os_getDevEui (u1_t* buf) {
  memcpy_P(buf, DEVEUI, 8);
}
void os_getDevKey (u1_t* buf) {
  memcpy_P(buf, APPKEY, 16);
}
static uint8_t sensorData[40];
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations). If no transmission has occurred in 10 TX intervals,
// the system should reset itself.
int unsigned TX_INTERVAL = 590;
const unsigned WDT_INTERVAL = (TX_INTERVAL * 10);

// Pin mapping for the particular Radiowing setup used by this project:
//   IO1->A (11)
//   RST->C (9)
//   CS ->D (6)
//   INT->E (5)
// Note that B (10) is used by the Adalogger featherwing for the SD card.
const lmic_pinmap lmic_pins = {
	.nss = RFM95_CS,
	.rxtx = LMIC_UNUSED_PIN,
	.rst = RFM95_RST,
	.dio = {RFM95_IRQ, RFM95_IO1, LMIC_UNUSED_PIN},
	.rxtx_rx_active = 0,
	.rssi_cal = 8,
	.spi_freq = 8000000,
};


void
do_send(osjob_t* j)
{
	// Check if there is not a current TX/RX job running
	if (LMIC.opmode & OP_TXRXPEND) {
		Serial.println("OP_TXRXPEND, not sending");
	} else {
		RecordMeasurement(sensorData);

		// Prepare upstream data transmission at the next possible time.
		LMIC_setTxData2(1, sensorData, 39, 1);
		Serial.println("Packet queued");
	}
	// Next TX is scheduled after TX_COMPLETE event.
}


#define DOWNLINK_DST	1
#define DOWNLINK_ENABLE	1


void
onEvent (ev_t ev)
{
	long int nextTX;
	Serial.print(os_getTime());
	Serial.print(": ");
	switch (ev) {
		case EV_SCAN_TIMEOUT:
			Serial.println("EV_SCAN_TIMEOUT");
			break;
		case EV_BEACON_FOUND:
			Serial.println("EV_BEACON_FOUND");
			break;
		case EV_BEACON_MISSED:
			Serial.println("EV_BEACON_MISSED");
			break;
		case EV_BEACON_TRACKED:
			Serial.println("EV_BEACON_TRACKED");
			break;
		case EV_JOINING:
			Serial.println("EV_JOINING");
			break;
		case EV_JOINED:
			Serial.println("EV_JOINED");
			{
				u4_t netid = 0;
				devaddr_t devaddr = 0;
				u1_t nwkKey[16];
				u1_t artKey[16];
				LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
				Serial.print("netid: ");
				Serial.println(netid, DEC);
				Serial.print("devaddr: ");
				Serial.println(devaddr, HEX);
				Serial.print("artKey: ");
				for (size_t i = 0; i < sizeof(artKey); ++i) {
					if (i != 0)
						Serial.print("-");
					Serial.print(artKey[i], HEX);
				}
				Serial.println("");
				Serial.print("nwkKey: ");
				for (size_t i = 0; i < sizeof(nwkKey); ++i) {
					if (i != 0)
						Serial.print("-");
					Serial.print(nwkKey[i], HEX);
				}
				Serial.println("");
			}
			// Disable link check validation (automatically enabled
			// during join, but because slow data rates change max TX
			// size, we don't use it in this example.
			LMIC_setLinkCheckMode(true);
			break;
			/*
			   || This event is defined but not used in the code. No
			   || point in wasting codespace on it.
			   ||
			   || case EV_RFU1:
			   ||     Serial.println("EV_RFU1");
			   ||     break;
			 */
		case EV_JOIN_FAILED:
			Serial.println("EV_JOIN_FAILED");
			break;
		case EV_REJOIN_FAILED:
			Serial.println("EV_REJOIN_FAILED");
			break;
		case EV_TXCOMPLETE:
			Serial.println("EV_TXCOMPLETE (includes waiting for RX windows)");
			if (LMIC.txrxFlags & TXRX_ACK)
				Serial.println("Received ack");

			// Downlink received with payload.
			if ((LMIC.dataLen) && (LMIC.dataLen != 0)) {
				// Don't process downlinks yet.
			}
			// Schedule next transmission
			nextTX = os_getTime() + sec2osticks(TX_INTERVAL);
			Serial.println("Next job scheduled for " + String(nextTX));
			os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
			break;
		case EV_LOST_TSYNC:
			Serial.println("EV_LOST_TSYNC");
			break;
		case EV_RESET:
			Serial.println("EV_RESET");
			break;
		case EV_RXCOMPLETE:
			// data received in ping slot
			Serial.println("EV_RXCOMPLETE");
			break;
		case EV_LINK_DEAD:
			Serial.println("EV_LINK_DEAD");
			break;
		case EV_LINK_ALIVE:
			Serial.println("EV_LINK_ALIVE");
			break;
			/*
			   || This event is defined but not used in the code. No
			   || point in wasting codespace on it.
			   ||
			   || case EV_SCAN_FOUND:
			   ||    Serial.println("EV_SCAN_FOUND");
			   ||    break;
			 */
		case EV_TXSTART:
			Serial.println("EV_TXSTART");
			break;
		case EV_JOIN_TXCOMPLETE:
			Serial.println("EV_JOIN_TXCOMPLETE");
			break;
		default:
			Serial.print("Unknown event: ");
			Serial.println((unsigned) ev);
			break;
	}
}


void
setup()
{
	// Reset radio
	digitalWrite(RFM95_RST, LOW);
	delay(100);

	Wire.begin();
	Wire.setClock(400000);

	Serial.begin(9600);
	Serial.println("BOOTING...");

	// LMIC init
	os_init();
	Serial.println("LMIC INIT: OK");

	// Reset the MAC state. Session and pending data transfers will be discarded.
	LMIC_reset();
	Serial.println("LMIC RESET: OK");

	LMIC_setLinkCheckMode(true);
	// LMIC_setAdrMode(true);
	LMIC_setDrTxpow(DR_SF9, 14);
	LMIC_selectSubBand(1);
	Serial.println("RFM95 SETUP: OK");

	SensorInit();

	Serial.println("BOOT OK");

	// Start job (sending automatically starts OTAA too)
	do_send(&sendjob);
	digitalWrite(LED_BUILTIN, HIGH);
}

void
loop()
{
	os_runloop_once();
}
