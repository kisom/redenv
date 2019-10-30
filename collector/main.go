package main

import (
	"database/sql"
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"net/http"
	"time"

	"github.com/kisom/redenv/collector/ttn"
	_ "github.com/lib/pq"
)

var (
	config *Config
	db     *sql.DB
)

const timeFormat = "2006-01-02 15:04:05 MST"

func httpError(w http.ResponseWriter, err error, code int) {
	log.Printf("[ERROR] %s", err)
	http.Error(w, err.Error(), http.StatusInternalServerError)
	return

}

func redenvCollector(w http.ResponseWriter, r *http.Request) {
	defer r.Body.Close()
	body, err := ioutil.ReadAll(r.Body)
	if err != nil {
	}

	var uplink ttn.Uplink
	err = json.Unmarshal(body, &uplink)
	if err != nil {
		httpError(w, err, http.StatusInternalServerError)
		return
	}

	reading, err := uplink.ToReading()
	if err != nil {
		httpError(w, err, http.StatusInternalServerError)
		return
	}

	err = StoreUplink(db, reading, uplink)
	if err != nil {
		httpError(w, err, http.StatusInternalServerError)
		return
	}

	log.Printf("reading from %s @ %s stored", uplink.DevID,
		reading.When.Format(timeFormat))
}

func index(w http.ResponseWriter, r *http.Request) {
	var timestamp int64

	row := db.QueryRow(`select recorded_at from readings order by recorded_at desc limit 1`)
	err := row.Scan(&timestamp)
	if err != nil {
		httpError(w, err, http.StatusInternalServerError)
		return
	}

	page := fmt.Sprintf(`fls-collector/web v1.0.0

last reading at: %s
`, time.Unix(timestamp, 0).Format(timeFormat))
	w.Write([]byte(page))
}

func main() {
	addr := "localhost:8006"
	configFile := "collector.conf"
	flag.StringVar(&addr, "a", addr, "`address` to listen on")
	flag.StringVar(&configFile, "f", configFile, "`path` to configuration file")
	flag.Parse()

	var err error
	config, err = LoadConfig(configFile)
	if err != nil {
		log.Fatal(err)
	}

	db, err = sql.Open("postgres", config.Database.ConnStr())
	if err != nil {
		log.Fatal(err)
	}
	defer db.Close()

	http.HandleFunc("/redenv/collector", redenvCollector)
	log.Printf("listening on %s", addr)
	log.Fatal(http.ListenAndServe(addr, nil))
}
