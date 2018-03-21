package lcm_go

// #cgo LDFLAGS: -llcm
//
// #include <stdlib.h>
// #include <string.h>
// #include <lcm/lcm.h>
//
// extern void goLCMCallbackHandler(void *, int, char *);
//
// static void lcm_msg_handler(const lcm_recv_buf_t *buffer, const char *channel,
//							   void *userdata) {
// 		(void)userdata;
// 		goLCMCallbackHandler((void *)buffer->data, (int)buffer->data_size,
// 							 (char *)channel);
// }
//
// static lcm_subscription_t * lcm_go_subscribe(lcm_t *lcm, const char *channel) {
// 		return lcm_subscribe(lcm, channel, &lcm_msg_handler, NULL);
// }
import "C"

import (
	"fmt"
	"unsafe"
)

var lcms []*LCM

type LCM struct {
	cLCM          *C.lcm_t
	subscriptions []*LCMSubscription
}

type LCMSubscription struct {
	channel          string
	handler          lcmHandler
	cLCMSubscription *C.lcm_subscription_t
}

type lcmHandler func([]byte, string)

func LCMCreate() (lcm *LCM, err error) {
	lcm = &LCM{}

	if lcm.cLCM = C.lcm_create(nil); lcm.cLCM == nil {
		err = fmt.Errorf("Could not create C lcm_t.")
		return nil, err
	}

	lcm.subscriptions = make([]*LCMSubscription, 0)

	lcms = append(lcms, lcm)

	return lcm, nil
}

//export goLCMCallbackHandler
func goLCMCallbackHandler(data unsafe.Pointer, size C.int, name *C.char) {
	channel := C.GoString(name)
	buffer := C.GoBytes(data, C.int(size))

	for _, lcm := range lcms {
		for _, subscription := range lcm.subscriptions {
			if channel == subscription.channel {
				subscription.handler(buffer, channel)
			}
		}
	}
}

func (lcm *LCM) Subscribe(channel string, handler lcmHandler) (*LCMSubscription, error) {
	subscription := &LCMSubscription{}
	subscription.channel = channel
	subscription.handler = handler

	cChannel := C.CString(channel)
	defer C.free(unsafe.Pointer(cChannel))

	subscription.cLCMSubscription = C.lcm_go_subscribe(lcm.cLCM, cChannel)
	if subscription.cLCMSubscription == nil {
		err := fmt.Errorf("Could not subscribe to channel %s.", channel)
		return nil, err
	}

	lcm.subscriptions = append(lcm.subscriptions, subscription)

	return subscription, nil
}

func (subscription *LCMSubscription) SetQueueCapacity(numMessages int) error {
	status := C.lcm_subscription_set_queue_capacity(subscription.cLCMSubscription,
		C.int(numMessages))

	if status != 0 {
		return fmt.Errorf("Could not change LCM queue capacity to %d.", numMessages)
	}

	return nil
}

func (lcm *LCM) Unsubscribe(subscription *LCMSubscription) error {
	status := C.lcm_unsubscribe(lcm.cLCM, subscription.cLCMSubscription)
	if status != 0 {
		return fmt.Errorf("Could not unsubsribe from %s.", subscription.channel)
	}

	return nil
}

func (lcm *LCM) Publish(channel string, data []byte) error {
	dataSize := C.size_t(len(data))

	buffer := C.malloc(dataSize)
	if buffer == nil {
		return fmt.Errorf("Could not malloc memory for lcm message.")
	}
	defer C.free(buffer)
	C.memcpy(buffer, unsafe.Pointer(&data[0]), dataSize)

	cChannel := C.CString(channel)
	defer C.free(unsafe.Pointer(cChannel))

	status := C.lcm_publish(lcm.cLCM, cChannel, buffer, C.uint(dataSize))
	if status == -1 {
		return fmt.Errorf("Could not publish LCM message: errorcode %v", status)
	}

	return nil
}

func (lcm *LCM) Handle() error {
	if status := C.lcm_handle(lcm.cLCM); status == -1 {
		return fmt.Errorf("Could not call lcm_handle")
	}

	return nil
}

func (lcm *LCM) Destroy() {
	C.lcm_destroy(lcm.cLCM)

	for i, l := range lcms {
		if l == lcm {
			lcms = append(lcms[:i], lcms[i+1:]...)
		}
	}
}
