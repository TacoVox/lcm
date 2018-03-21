package lcm_go

import (
	"testing"
	"time"
)

var handle bool = true
var messages int = 1
var messageGoal int = 10000
var someBytes []byte = []byte("abcdefghijklmnopqrstuvwxyz")

func testHandler(data []byte, channel string) {
	messages++
}

func TestLCM(t *testing.T) {
	var err error
	var lcm *LCM
	var subscription *LCMSubscription

	// New LCM
	if lcm, err = LCMInstance(); err != nil {
		t.Fatal(err)
	}

	// Subscribe to "TEST" LCM channel using Go
	if subscription, err = lcm.Subscribe("TEST", testHandler); err != nil {
		t.Fatal(err)
	}

	// Increase the queue capacity
	if err = subscription.SetQueueCapacity(100); err != nil {
		t.Fatal(err)
	}

	// Wait until we receive some data
	go func() {
		// Listen for some data
		for true {
			if err = lcm.Handle(); err != nil {
				t.Fatal(err)
			}
		}
	}()

	// Send X messages
	for i := 1; i < messageGoal; i++ {
		if err = lcm.Publish("TEST", someBytes); err != nil {
			t.Fatal(err)
		}
	}

	// Wait until we either get X messages or time out after 5 seconds
	secs := time.Now().Unix()
	for messages != messageGoal && time.Now().Unix()-secs < 5 {
		//Wait
	}

	// Did we receive X messages?
	if messages != messageGoal {
		t.Fatalf("Expected %d but received %d messages.", messageGoal, messages)
	}

	// Wrap up by unsubscribing
	if err = lcm.Unsubscribe(subscription); err != nil {
		t.Fatal(err)
	}
}
