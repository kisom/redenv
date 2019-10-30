package main

import (
	"errors"
	"fmt"
	"strconv"

	"github.com/gokyle/goconfig"
)

type TTN struct {
	app_id     string
	access_key string
	auth_key   string
}

func TTNFromMap(cfg map[string]string) (TTN, error) {
	ttn := TTN{}

	ttn.app_id = cfg["app_id"]
	ttn.access_key = cfg["access_key"]
	ttn.auth_key = cfg["auth_key"]

	return ttn, ttn.Validate()
}

func (ttn TTN) Validate() error {
	missing := func(v string) error {
		return fmt.Errorf("collector: ttn config is missing %s", v)
	}

	if ttn.app_id == "" {
		return missing("app_id")
	}

	if ttn.access_key == "" {
		return missing("access_key")
	}

	if ttn.auth_key == "" {
		return missing("auth_key")
	}

	return nil
}

func (ttn TTN) AppID() string     { return ttn.app_id }
func (ttn TTN) AccessKey() string { return ttn.access_key }
func (ttn TTN) AuthKey() string   { return ttn.auth_key }

type Database struct {
	user     string
	password string
	host     string
	port     int
	name     string
}

func (db Database) Validate() error {
	missing := func(v string) error {
		return fmt.Errorf("collector: database config is missing %s", v)
	}

	if db.user == "" {
		return missing("user")
	}

	if db.password == "" {
		return missing("password")
	}

	if db.host == "" {
		return missing("host")
	}

	if db.port == 0 {
		return missing("port")
	}

	if db.name == "" {
		return missing("name")
	}

	return nil
}

func DatabaseFromMap(cfg map[string]string) (Database, error) {
	var err error

	db := Database{port: 5432}
	db.user = cfg["user"]
	db.password = cfg["password"]
	db.host = cfg["host"]
	db.name = cfg["name"]

	if port, ok := cfg["port"]; ok {
		db.port, err = strconv.Atoi(port)
	}

	if err == nil {
		err = db.Validate()
	}
	return db, err
}

func (db Database) User() string     { return db.user }
func (db Database) Password() string { return db.password }
func (db Database) Host() string     { return db.host }
func (db Database) Port() int        { return db.port }
func (db Database) Name() string     { return db.name }

func (db Database) ConnStr() string {
	return fmt.Sprintf("postgres://%s:%s@%s:%d/%s?sslmode=verify-full",
		db.user, db.password, db.host, db.port, db.name)
}

type Config struct {
	TTN      TTN
	Database Database
}

func LoadConfig(path string) (*Config, error) {
	config := &Config{}

	cfgMap, err := goconfig.ParseFile(path)
	if err != nil {
		return nil, err
	}

	if !cfgMap.SectionInConfig("ttn") {
		return nil, errors.New("collector: config is missing ttn section")
	}

	if !cfgMap.SectionInConfig("database") {
		return nil, errors.New("collector: config is missing database section")
	}

	config.TTN, err = TTNFromMap(cfgMap["ttn"])
	if err != nil {
		return nil, err
	}

	config.Database, err = DatabaseFromMap(cfgMap["database"])
	if err != nil {
		return nil, err
	}

	return config, nil
}
