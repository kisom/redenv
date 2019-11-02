package main

import (
	"database/sql"

	"github.com/google/uuid"
	"github.com/kisom/redenv/collector/reading"
	"github.com/kisom/redenv/collector/ttn"
)

var (
	insertReading = `INSERT INTO readings (
	received_at,
	device,
	uplink,
	recorded_at,
	hardware,
	uptime,
	temperature,
	temperature_cal,
	temperature_is_cal,
	humidity,
	pressure,
	ccs811_status,
	co2,
	tvoc,
	voltage
) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15)`
	insertUplink = `INSERT INTO uplinks (
	id,
	app_id,
	dev_id,
	hw_serial,
	port,
	counter,
	is_retry,
	is_confirmed,
	payload_raw,
	uplink_time,
	frequency,
	modulation,
	data_rate,
	bit_rate
) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)`
)

func StoreUplink(db *sql.DB, r *reading.Reading, u *ttn.Uplink) error {
	id, err := uuid.NewRandom()
	if err != nil {
		return err
	}

	r.Uplink = id.String()

	tx, err := db.Begin()
	if err != nil {
		return err
	}

	_, err = tx.Exec(insertUplink, id.String(), u.AppID, u.DevID, u.HardwareSerial,
		u.Port, u.Counter, u.IsRetry, u.Confirmed, u.PayloadRaw,
		r.ReceivedAt.Unix(), u.Metadata.Frequency, u.Metadata.Modulation,
		u.Metadata.DataRate, u.Metadata.BitRate)
	if err != nil {
		// TODO: Could be a doule error, but not worth figuring out right now.
		tx.Rollback()
		return err
	}

	_, err = tx.Exec(insertReading, r.ReceivedAt.Unix(), r.Device, r.Uplink,
		r.When.Unix(), r.Hardware, r.Uptime,
		r.Temperature, r.TemperatureCalibration, r.TemperatureCalibrated,
		r.Humidity, r.Pressure,
		r.CCS811Status, r.CO2, r.TVOC, r.Voltage)
	if err != nil {
		// TODO: Could be a doule error, but not worth figuring out right now.
		tx.Rollback()
		return err
	}

	return tx.Commit()
}
