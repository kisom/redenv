package reading

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/kisom/redenv/collector/util"
)

const ReadingSize = 41

var Timezone *time.Location

func init() {
	var err error
	Timezone, err = time.LoadLocation("America/Los_Angeles")
	if err != nil {
		panic(err.Error())
	}
}

var (
	ErrCCS811ID       = errors.New("ccs811: invalid ID")
	ErrCCS811I2C      = errors.New("ccs811: I2C error")
	ErrCCS811Internal = errors.New("ccs811: internal error")
	ErrCCS811Generic  = errors.New("ccs811: generic error")
	ErrCCS811Unknown  = errors.New("ccs811: unknown error")
)

func statusToCCS811Error(status uint8) error {
	switch status {
	case 0:
		return nil
	case 1:
		return ErrCCS811ID
	case 2:
		return ErrCCS811I2C
	case 3:
		return ErrCCS811Internal
	case 4:
		return ErrCCS811Generic
	default:
		return ErrCCS811Unknown
	}
}

func statusToCCS811String(status uint8) string {
	switch status {
	case 0:
		return "OK"
	case 1:
		return "invalid ID"
	case 2:
		return "I2C error"
	case 3:
		return "internal error"
	case 4:
		return "generic error"
	default:
		return "unknown error or sensor is off"
	}
}

const (
	HardwareBME280 uint8 = 1 << iota
	HardwareCCS811
	HardwareRTC
	HardwareSD
	HardwareGPS
)

type Reading struct {
	ID         string
	ReceivedAt time.Time
	Device     string
	Uplink     string

	When     time.Time
	Hardware uint8
	Uptime   uint32

	Temperature            float32
	TemperatureCalibration float32
	TemperatureCalibrated  bool
	Humidity               float32
	Pressure               float32

	CCS811Status uint8
	CCS811Error  error
	CO2          int32
	TVOC         int32

	Voltage uint8
	Fix     bool
	Sats    uint8
}

func ccs811Reading(v int32, unit string) string {
	if v == -1 {
		return "not recorded"
	}
	return fmt.Sprintf("%d %s", v, unit)
}

func (r Reading) String() string {
	uptime := time.Duration(r.Uptime) * time.Second
	return fmt.Sprintf(`	Recorded: %s
	Hardware: %s
	Uptime: %s
	Temperature: %0.2f°C
	Temperature calibration: %0.2f°C
	Temperature calibrated? %s
	Humidity: %0.2f
	Barometric pressure: %0.4f kPa
	CCS811 status: %s
	CO2: %s
	TVOC: %s
	Voltage: %0.1fV (note voltages over 10V may not be accurate)
	Sats: %d (fix? %s)
`,
		r.When.In(Timezone).Format(util.TimeFormat),
		r.HardwareAsString(),
		uptime,
		r.Temperature,
		r.TemperatureCalibration,
		util.YOrN(r.TemperatureCalibrated),
		r.Humidity,
		r.Pressure/1000.0,
		statusToCCS811String(r.CCS811Status),
		ccs811Reading(r.CO2, "ppm"),
		ccs811Reading(r.TVOC, "ppb"),
		r.VoltageF(),
		r.Sats,
		util.YOrN(r.Fix),
	)

}

func (r Reading) VoltageF() float32 {
	return float32(r.Voltage) / 100.0
}

func (r Reading) HardwareAsString() string {
	hw := make([]string, 0, 5)

	if r.Hardware&HardwareBME280 != 0 {
		hw = append(hw, "BME280")
	}

	if r.Hardware&HardwareCCS811 != 0 {
		hw = append(hw, "CCS811")
	}

	if r.Hardware&HardwareRTC != 0 {
		hw = append(hw, "RTC")
	}

	if r.Hardware&HardwareSD != 0 {
		hw = append(hw, "SD")
	}

	if r.Hardware&HardwareGPS != 0 {
		hw = append(hw, "GPS")
	}

	return strings.Join(hw, ",")
}

func (r *Reading) Unmarshal(data []byte) error {
	if len(data) != ReadingSize {
		return errors.New("collector: invalid reading raw data length")
	}

	buf := bytes.NewBuffer(data)

	var year uint16
	var month, day, hour, minute, second uint8
	var calibrated uint8

	read := func(value interface{}) error {
		return binary.Read(buf, binary.LittleEndian, value)
	}

	if err := read(&year); err != nil {
		return err
	}

	if err := read(&month); err != nil {
		return err
	}

	if err := read(&day); err != nil {
		return err
	}

	if err := read(&hour); err != nil {
		return err
	}

	if err := read(&minute); err != nil {
		return err
	}

	if err := read(&second); err != nil {
		return err
	}

	if err := read(&r.Hardware); err != nil {
		return err
	}

	if err := read(&r.Uptime); err != nil {
		return err
	}

	if err := read(&r.Temperature); err != nil {
		return err
	}

	if err := read(&r.TemperatureCalibration); err != nil {
		return err
	}

	if err := read(&r.Humidity); err != nil {
		return err
	}

	if err := read(&r.Pressure); err != nil {
		return err
	}

	if err := read(&r.CO2); err != nil {
		return err
	}

	if err := read(&r.TVOC); err != nil {
		return err
	}

	if err := read(&r.Voltage); err != nil {
		return err
	}

	if err := read(&r.CCS811Status); err != nil {
		return err
	}

	if err := read(&calibrated); err != nil {
		return err
	}

	if calibrated == 1 {
		r.TemperatureCalibrated = true
	}

	var fix uint8
	if err := read(&fix); err != nil {
		return err
	}
	if fix == 1 {
		r.Fix = true
	}

	if err := read(&r.Sats); err != nil {
		return err
	}

	r.CCS811Error = statusToCCS811Error(r.CCS811Status)
	r.When = time.Date(int(year), time.Month(month), int(day), int(hour), int(minute), int(second), 0, time.UTC)
	return nil
}
