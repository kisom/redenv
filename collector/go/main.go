package main

import (
	"fmt"
	"os"

	ttnsdk "github.com/TheThingsNetwork/go-app-sdk"
	ttnlog "github.com/TheThingsNetwork/go-utils/log"
	"github.com/TheThingsNetwork/go-utils/log/apex"
)

const (
	sdkClientName = "fls-collector"
	//sdkClientVersion = "1.0.0"
	sdkClientVersion = "2.0.5"
	handlerAddress   = "us-west.thethings.network:1904"
)

func main() {
	log := apex.Stdout() // We use a cli logger at Stdout
	log.MustParseLevel("debug")
	ttnlog.Set(log) // Set the logger as default for TTN

	// We get the application ID and application access key from the environment
	appID := os.Getenv("TTN_APP_ID")
	appAccessKey := os.Getenv("TTN_APP_ACCESS_KEY")

	config := ttnsdk.NewCommunityConfig(sdkClientName)
	config.ClientVersion = sdkClientVersion
	config.HandlerAddress = handlerAddress

	log.Info("setting up client")
	// Create a new SDK client for the application
	client := config.NewClient(appID, appAccessKey)

	// Make sure the client is closed before the function returns
	// In your application, you should call this before the application shuts down
	defer client.Close()

	log.Info("Starting pubsub client")

	// Start Publish/Subscribe client (MQTT)
	pubsub, err := client.PubSub()
	if err != nil {
		log.WithError(err).Fatal("fls-collector: could not get application pub/sub")
	}

	// Make sure the pubsub client is closed before the function returns
	// In your application, you should call this before the application shuts down
	defer pubsub.Close()

	log.Info("starting pubsub for all clients")
	// Get a publish/subscribe client for all devices
	allDevicesPubSub := pubsub.AllDevices()

	// Subscribe to events
	events, err := allDevicesPubSub.SubscribeEvents()
	if err != nil {
		log.WithError(err).Fatal("fls-collector: could not subscribe to events")
	}
	log.Debug("After this point, the program won't show anything until we receive an application event.")
	for event := range events {
		log.WithFields(ttnlog.Fields{
			"devID":     event.DevID,
			"eventType": event.Event,
		}).Info("my-amazing-app: received event")
		if event.Data != nil {
			fmt.Println(event.Data)
		}
		break // normally you wouldn't do this
	}

	// Unsubscribe from events
	err = allDevicesPubSub.UnsubscribeEvents()
	if err != nil {
		log.WithError(err).Fatal("fls-collector: could not unsubscribe from events")
	}
}
