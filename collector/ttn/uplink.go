package ttn

import (
	"encoding/base64"
	"fmt"
	"time"

	"github.com/kisom/redenv/collector/reading"
	"github.com/kisom/redenv/collector/util"
)

type Uplink struct {
	AppID          string   `json:"app_id"`
	DevID          string   `json:"dev_id"`
	HardwareSerial string   `json:"hardware_serial"`
	Port           int      `json:"port"`
	Counter        int      `json:"counter"`
	IsRetry        bool     `json:"is_retry"`
	Confirmed      bool     `json:"confirmed"`
	PayloadRaw     string   `json:"payload_raw"`
	Metadata       Metadata `json:"metadata"`
}

type Metadata struct {
	Time       string  `json:"time"`
	Frequency  float32 `json:"frequency"`
	Modulation string  `json:"modulation"`
	DataRate   string  `json:"data_rate"`
	BitRate    string  `json:"bit_rate"`
}

func (u Uplink) ToReading() (*reading.Reading, error) {
	r := &reading.Reading{}
	r.Device = u.DevID

	payload, err := base64.StdEncoding.DecodeString(u.PayloadRaw)
	if err != nil {
		return nil, err
	}

	r.ReceivedAt, err = time.Parse(time.RFC3339, u.Metadata.Time)
	if err != nil {
		// When getting uplinks from the database, the time is
		// stored in a slightly saner format.
		r.ReceivedAt, err = time.Parse(util.TimeFormat, u.Metadata.Time)
		if err != nil {
			return nil, err
		}
	}
	err = r.Unmarshal(payload)
	return r, err
}

func (u Uplink) String() string {
	r := &reading.Reading{}
	r.Device = u.DevID

	payload, err := base64.StdEncoding.DecodeString(u.PayloadRaw)
	if err != nil {
		// things should have been validated by here, and if
		// you haven't then the app deserves to crash
		panic(err)
	}

	r.ReceivedAt, err = time.Parse(time.RFC3339, u.Metadata.Time)
	if err != nil {
		// When getting uplinks from the database, the time is
		// stored in a slightly saner format.
		r.ReceivedAt, err = time.Parse(util.TimeFormat, u.Metadata.Time)
		if err != nil {
			// things should have been validated by here,
			// and if you haven't then the app deserves to
			// crash
			panic(err)
		}
	}
	err = r.Unmarshal(payload)
	if err != nil {
		// things should have been validated by here, and if
		// you haven't then the app deserves to crash
		panic(err)
	}

	return fmt.Sprintf(`Uplink[%s] from %s[%s] @ %s
	Port: %d
	Counter: %d
	Retry? %s
	Confirmed? %s
	Frequency: %0.3f
	Modulation: %s
	Data rate: %s
	Bit rate: %s
Reading:
%s

`, u.AppID, u.DevID, u.HardwareSerial, u.Metadata.Time, u.Port,
		u.Counter, util.YOrN(u.IsRetry), util.YOrN(u.Confirmed),
		u.Metadata.Frequency, u.Metadata.Modulation,
		u.Metadata.DataRate, u.Metadata.BitRate, r)
}

/*
{
  "app_id": "my-app-id",              // Same as in the topic
  "dev_id": "my-dev-id",              // Same as in the topic
  "hardware_serial": "0102030405060708", // In case of LoRaWAN: the DevEUI
  "port": 1,                          // LoRaWAN FPort
  "counter": 2,                       // LoRaWAN frame counter
  "is_retry": false,                  // Is set to true if this message is a retry (you could also detect this from the counter)
  "confirmed": false,                 // Is set to true if this message was a confirmed message
  "payload_raw": "AQIDBA==",          // Base64 encoded payload: [0x01, 0x02, 0x03, 0x04]
  "payload_fields": {},               // Object containing the results from the payload functions - left out when empty
  "metadata": {
    "time": "1970-01-01T00:00:00Z",   // Time when the server received the message
    "frequency": 868.1,               // Frequency at which the message was sent
    "modulation": "LORA",             // Modulation that was used - LORA or FSK
    "data_rate": "SF7BW125",          // Data rate that was used - if LORA modulation
    "bit_rate": 50000,                // Bit rate that was used - if FSK modulation
    "coding_rate": "4/5",             // Coding rate that was used
    "gateways": [
      {
        "gtw_id": "ttn-herengracht-ams", // EUI of the gateway
        "timestamp": 12345,              // Timestamp when the gateway received the message
        "time": "1970-01-01T00:00:00Z",  // Time when the gateway received the message - left out when gateway does not have synchronized time
        "channel": 0,                    // Channel where the gateway received the message
        "rssi": -25,                     // Signal strength of the received message
        "snr": 5,                        // Signal to noise ratio of the received message
        "rf_chain": 0,                   // RF chain where the gateway received the message
        "latitude": 52.1234,             // Latitude of the gateway reported in its status updates
        "longitude": 6.1234,             // Longitude of the gateway
        "altitude": 6                    // Altitude of the gateway
      },
      //...more if received by more gateways...
    ],
    "latitude": 52.2345,              // Latitude of the device
    "longitude": 6.2345,              // Longitude of the device
    "altitude": 2                     // Altitude of the device
  },
  "downlink_url": "https://integrations.thethingsnetwork.org/ttn-eu/api/v2/down/my-app-id/my-process-id?key=ttn-account-v2.secret"
}

*/
