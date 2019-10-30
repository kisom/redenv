package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"strings"
	"time"
)

const ReadingSize = 39

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

const (
	HardwareBME280 uint8 = 1 << iota
	HardwareCCS811
	HardwareRTC
	HardwareSD
)

type Reading struct {
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

	voltage uint8
}

func (r Reading) Voltage() float32 {
	return float32(r.voltage) / 10.0
}

func (r Reading) HardwareAsString() string {
	hw := make([]string, 0, 4)

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

	if err := read(&r.voltage); err != nil {
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

	r.CCS811Error = statusToCCS811Error(r.CCS811Status)
	r.When = time.Date(int(year), time.Month(month), int(day), int(hour), int(minute), int(second), 0, Timezone)
	return nil
}
