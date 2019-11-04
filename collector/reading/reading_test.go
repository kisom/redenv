package reading

import (
	"fmt"
	"testing"
	"time"

	"github.com/kisom/goutils/assert"
)

func fleq(a, b float32) bool {
	c := a - b
	if c < 0 {
		c = 0 - c
	}

	if c <= 0.2 {
		return true
	}

	return false
}

func TestUnmarshal0(t *testing.T) {
	var data = []byte{
		0xE3, 0x07, 0x0A, 0x1D, 0x10, 0x0F, 0x17, 0x0D,
		0x00, 0x00, 0x00, 0x00, 0x7B, 0x14, 0xD2, 0x41,
		0x00, 0x00, 0x00, 0x00, 0x00, 0xD8, 0x98, 0x41,
		0x07, 0xF7, 0xC5, 0x47, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xCC, 0xFF, 0x00, 0x00,
		0x00,
	}

	assert.BoolT(t, len(data) == ReadingSize, "invalid data length")

	var reading = &Reading{}
	err := reading.Unmarshal(data)
	assert.NoErrorT(t, err)

	expectedDate := time.Date(2019, 10, 29, 16, 15, 23, 0, time.UTC)
	assert.BoolT(t, reading.When.Equal(expectedDate))
	assert.BoolT(t, reading.Hardware == 13)
	assert.BoolT(t, reading.Uptime == 0)
	assert.BoolT(t, fleq(reading.Temperature, 26.3), "temperature")
	assert.BoolT(t, fleq(reading.TemperatureCalibration, 0.0), "temperature calibration")
	assert.BoolT(t, !reading.TemperatureCalibrated, "temperature calibrated")
	assert.BoolT(t, fleq(reading.Humidity, 19.1), "humidity")
	assert.BoolT(t, fleq(reading.Pressure, 101358.0), "pressure")
	assert.BoolT(t, reading.CO2 == -1, "CO2")
	assert.BoolT(t, reading.TVOC == -1, "TVOC")
	assert.BoolT(t, reading.Voltage == 204, "voltage")
	assert.BoolT(t, reading.CCS811Status == 255, "CCS811 status")
	assert.BoolT(t, reading.CCS811Error == ErrCCS811Unknown, "CCS811 error")
	assert.BoolT(t, !reading.Fix, "GPS fix")
	assert.BoolT(t, reading.Sats == 0, "GPS sats")

	assert.BoolT(t, reading.HardwareAsString() == "BME280,RTC,SD", "hardware string")
	assert.BoolT(t, fleq(reading.VoltageF(), 2.04), "voltage float", fmt.Sprintf("%f", reading.VoltageF()))
}

func TestUnmarshal1(t *testing.T) {
	var data = []byte{
		0xE3, 0x07, 0x0A, 0x1D, 0x0B, 0x1E, 0x04, 0x0F,
		0xAB, 0x08, 0x00, 0x00, 0xAF, 0x47, 0x9D, 0x41,
		0x1C, 0x85, 0xF3, 0x40, 0x00, 0xAA, 0xF4, 0x41,
		0xC2, 0x7B, 0xC6, 0x47, 0x3F, 0x02, 0x00, 0x00,
		0x1A, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x01, 0x00,
		0x00,
	}

	assert.BoolT(t, len(data) == ReadingSize, "invalid data length")

	var reading = &Reading{}
	err := reading.Unmarshal(data)
	assert.NoErrorT(t, err)

	expectedDate := time.Date(2019, 10, 29, 11, 30, 4, 0, time.UTC)
	assert.BoolT(t, reading.When.Equal(expectedDate), "timestamp")
	assert.BoolT(t, reading.Hardware == 15, "hardware")
	assert.BoolT(t, reading.Uptime == 2219, "uptime")
	assert.BoolT(t, fleq(reading.Temperature, 19.66), "temperature")
	assert.BoolT(t, fleq(reading.TemperatureCalibration, 7.61), "temperature calibration")
	assert.BoolT(t, reading.TemperatureCalibrated, "temperature calibrated")
	assert.BoolT(t, fleq(reading.Humidity, 30.58), "humidity")
	assert.BoolT(t, fleq(reading.Pressure, 101623.52), "pressure")
	assert.BoolT(t, reading.CO2 == 575, "CO2")
	assert.BoolT(t, reading.TVOC == 26, "TVOC")
	assert.BoolT(t, reading.Voltage == 253, "voltage")
	assert.BoolT(t, reading.CCS811Status == 0, "CCS811 status")
	assert.NoErrorT(t, reading.CCS811Error)
	assert.BoolT(t, !reading.Fix, "GPS fix")
	assert.BoolT(t, reading.Sats == 0, "GPS sats")

	assert.BoolT(t, reading.HardwareAsString() == "BME280,CCS811,RTC,SD", "hardware string")
	assert.BoolT(t, fleq(reading.VoltageF(), 2.53), "voltage float", fmt.Sprintf("%f", reading.VoltageF()))
}
