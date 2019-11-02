BEGIN;

CREATE TABLE uplinks (
	id			UUID PRIMARY KEY DEFAULT gen_random_uuid(),
	app_id			TEXT NOT NULL,
	dev_id			TEXT NOT NULL,
	hw_serial		TEXT NOT NULL,
	port			INTEGER NOT NULL,
	counter			INTEGER NOT NULL,
	is_retry		BOOLEAN NOT NULL,
	is_confirmed		BOOLEAN NOT NULL,
	payload_raw		TEXT NOT NULL,
	uplink_time		INTEGER NOT NULL,
	frequency		FLOAT NOT NULL,
	modulation		TEXT NOT NULL DEFAULT 'simulated',
	data_rate		TEXT NOT NULL DEFAULT 'simulated',
	bit_rate		TEXT NOT NULL DEFAULT 'simulated',
	unique(dev_id, hw_serial, payload_raw)
);

CREATE TABLE readings (
       -- RECORD HEADER
       id			UUID PRIMARY KEY DEFAULT gen_random_uuid(),
       received_at		INTEGER NOT NULL,
       device			TEXT NOT NULL,
       uplink			UUID REFERENCES uplinks,

       -- PAYLOAD HEADER
       recorded_at		INTEGER NOT NULL,
       hardware			INTEGER NOT NULL,
       uptime			INTEGER NOT NULL,

       -- BME280
       temperature		FLOAT NOT NULL,
       temperature_cal		FLOAT NOT NULL,
       temperature_is_cal	BOOLEAN NOT NULL,
       humidity			FLOAT NOT NULL,
       pressure			FLOAT NOT NULL,

       -- CCS811
       ccs811_status		INTEGER NOT NULL,
       co2			INTEGER NOT NULL,
       tvoc			INTEGER NOT NULL,

       -- solar cell
       voltage			INTEGER NOT NULL
);

COMMIT;
