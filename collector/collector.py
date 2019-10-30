#!/usr/bin/env python3
"""
Collector sets up a pubsub connection to the Things Network, receiving
uplinks as they are transmitted and writes them to the wntrmute
database.
"""

import base64
import datetime
import logging
import os
import struct
import time
import uuid

import pytz
import ttn
from sqlalchemy import create_engine
from sqlalchemy import Float, Column, Integer, String
from sqlalchemy.dialects.postgresql import BOOLEAN, BYTEA, UUID
from sqlalchemy.orm import sessionmaker
from sqlalchemy.ext.declarative import declarative_base

TIMEZONE = pytz.timezone(zone="America/Los_Angeles")
DATABASE = None

def load_env(path=".env"):
    """Load config from env file because systemd is dumb."""
    config = {}
    with open(path, 'rt') as env:
        for line in env:
            key, value = line.strip().split('=', 1)
            config[key] = value
    return config


def parse_timestamp(tstr):
    """
    TTN sends a timestamp in a time format that uses nanoseconds, which
    aren't useful. This function clamps it down to milliseconds.
    """
    ts1, ts2 = tstr.split(".")
    ts2 = ts2[:3]  # nanoseconds -> milliseconds
    return datetime.datetime.strptime(ts1 + "." + ts2, "%Y-%m-%dT%H:%M:%S.%f")


class Database:
    """
    Database stores a database connection alongside a session maker. It's
    really more of a struct than a class.
    """

    __slots__ = ["db", "session"]

    def __init__(self, connstr):
        """Connect to postgres given a connstring."""
        self.db = create_engine(connstr, pool_size=20, max_overflow=0)
        self.session = sessionmaker()
        self.session.configure(bind=self.db)
        self.db.connect()


def get_db(config):
    """
    Collect the database credentials from the environment, and use that
    to set up a database.
    """
    logging.info(f"attempting to connect to database host {config['DB_HOST']}")
    connstr = f"postgres://{config['DB_USER']}:{config['DB_PASSWORD']}@{config['DB_HOST']}:5432/{config['DB_NAME']}"
    return Database(connstr)


class Reading(declarative_base()):
    """
    Reading stores a single measurement from a redenv node, alongside
    some metadata.
    """

    __tablename__ = "readings"

    id = Column(UUID, primary_key=True)
    received_at = Column(Integer, nullable=False)
    device = Column(String, nullable=False)
    payload = Column(BYTEA, nullable=False)

    # Header
    recorded_at = Column(Integer, nullable=False)
    hardware = Column(Integer, nullable=False)
    uptime = Column(Integer, nullable=False)

    # BME280
    temperature = Column(Float, nullable=False)
    temperature_cal = Column(Float, nullable=False)
    temperature_is_cal = Column(BOOLEAN, nullable=False)
    humidity = Column(Float, nullable=False)
    pressure = Column(Float, nullable=False)

    # CCS811
    ccs811_status = Column(Integer, nullable=False)
    co2 = Column(Integer, nullable=False)
    tvoc = Column(Integer, nullable=False)

    # Solar cell
    voltage = Column(Integer, nullable=False)


def uplink_callback(msg, _client):
    """
    When a new message comes in from a node, do some data prep and
    store the record in the database.
    """
    logging.info(f"Received uplink from {msg.dev_id}")
    logging.debug(msg)

    # MSG(app_id='fls',
    #     dev_id='backyard',
    #     hardware_serial='009CB1747141CD05',
    #     port=1, counter=0,
    #     payload_raw='4wcKHRAPFw0AAAAAexTSQQAAAAAA2JhBB/fFR///////////zP8A',
    #     metadata=MSG(time='2019-10-30T02:10:27.094175318Z'))

    timestamp = parse_timestamp(msg.metadata.time)
    payload = base64.decodebytes(msg.payload_raw.encode("utf-8"))
    data = struct.unpack("<HBBBBBBIffffiiBBB", payload)
    record_when = datetime.datetime(
        data[0], data[1], data[2], data[3], data[4], data[5], 0, TIMEZONE
    )
    calibrated = False
    if data[16] == 1:
        calibrated = True
    record = Reading(
        id=str(uuid.uuid4()),
        received_at=int(timestamp.timestamp()),
        device=msg.dev_id,
        payload=payload,
        recorded_at=record_when.timestamp(),
        hardware=data[6],
        uptime=data[7],
        temperature=data[8],
        temperature_cal=data[9],
        temperature_is_cal=calibrated,
        humidity=data[10],
        pressure=data[11],
        co2=data[12],
        tvoc=data[13],
        voltage=data[14],
        ccs811_status=data[15],
    )

    logging.info("connecting to database")
    session = DATABASE.session()
    session.add(record)
    logging.info("record added")
    session.commit()
    logging.info(f"stored record from {msg.dev_id}")


def main(config, block=False):
    """
    Set up a TTN pubsub connection and register a callback. If
    block is True, the function will enter into a sleeping
    infinite loop.
    """
    app_id = config["TTN_APP_ID"]
    access_key = config["TTN_APP_ACCESS_KEY"]
    assert app_id is not None
    assert access_key is not None
    handler = ttn.HandlerClient(app_id, access_key)

    mqtt_client = handler.data()
    mqtt_client.set_uplink_callback(uplink_callback)

    if not block:
        return mqtt_client

    logging.info("connecting MQTT client")
    try:
        mqtt_client.connect()
        logging.info("waiting on new connections")
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logging.info("keyboard interrupt caught, exiting")
        return None
    except Exception as exc:
        logging.exception(exc)
    finally:
        logging.info("closing existing MQTT client")
        mqtt_client.close()


if __name__ == "__main__":
    logging.basicConfig(
        format="fls-collector(%(levelname)s):%(message)s", level=logging.INFO
    )
    CONFIG = load_env()
    DATABASE = get_db(CONFIG)
    while True:
        main(CONFIG, block=True)
