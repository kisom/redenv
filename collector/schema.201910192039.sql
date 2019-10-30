BEGIN;

CREATE TABLE readings (
       -- RECORD HEADER
       id			UUID PRIMARY KEY DEFAULT gen_random_uuid(),
       received_at		INTEGER NOT NULL,
       device			TEXT NOT NULL,
       payload			BYTEA NOT NULL,

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
       voltage			INTEGER NOT NULL,
       unique(device, payload)
);

COMMIT;
