package sctp

// #cgo CFLAGS: -g -Wall -I../core/src
// #cgo LDFLAGS: -L${SRCDIR}/core/lib -lsctpcore
// #include <stdlib.h>
// #include "adapter.h"
// extern void receiveMultiMessage(int fd, void* args);
// static poller_action_t *poller_add_receiver(poller_t *poller, int fd, void *args) {
//     poller_action_t *action = calloc(1, sizeof(poller_action_t));
//     action->fd = fd;
//     action->cb = receiveMultiMessage;
//     action->args = args;
//	   if (poller_add(poller, action)) {
//	      return NULL;
//	   }
//
//     return action;
// }
import "C"
import (
	"context"
	"fmt"
	"sync"
	"time"
	"unsafe"
)

// This is a wrapper structure for an epoll implemented in C.
//
// This intended to be used as a pool of pollers which monitor multiple sockets.
// If we want to handle messages received from various sockets
// monitored by a poller, then we need to register a go function
// as an action callback.
// The poller wakes up when there's an event happening for an FD.
// It then executes the registered callback for the given FD.
//
// This approach might have some problems.
// Further exploration topics:
// - thundering herd
// - thread starvation
// - if the callback functions block, they will block the poller.
type Receiver struct {
	wg     sync.WaitGroup
	poller *C.struct_poller
	mmsg   *C.struct_mmsg
	// TODO: store an array for added actions. free them on Close().
}

func NewReceiver(timeout int) *Receiver {
	mmsg := CreateMultiMsg(10, 9216)
	return &Receiver{
		wg:     sync.WaitGroup{},
		poller: C.poller_create(C.int(timeout)),
		mmsg:   mmsg,
	}
}

// https://stackoverflow.com/questions/6125683/call-go-functions-from-c

//export receiveMultiMessage
func receiveMultiMessage(fd C.int, args unsafe.Pointer) {
	mmsg := (*C.struct_mmsg)(args)
	// TODO: can I propagate the main context up to here?
	// I tried to wrap the context in the args, but cgo complained
	// about passing Go pointers to Go pointers.
	// panic: runtime error: cgo argument has Go pointer to Go pointer
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	// TODO: is it ok to timeout? does it mean there's nothing to read?
	numMsg, err := RecvMultiMsg(ctx, int(fd), mmsg)
	if err != nil {
		fmt.Printf("error reading message: %s", err.Error())
		return
	}
	mmsgit := GetMultiMsgIterator(mmsg)
	bytes := mmsgit.Next()
	for numMsg > 0 {
		// TODO: this is example code; Printf is commented out to not
		// pollute stdout.
		//fmt.Printf("received buf: %s, len: %d\n",
		//	string(bytes), len(bytes))
		bytes = mmsgit.Next()
		_ = bytes
		numMsg--
	}
}

// TODO: what happens if the same FD is added multiple times?
func (p *Receiver) Add(fd int) error {
	action := C.poller_add_receiver(
		p.poller,
		C.int(fd),
		(unsafe.Pointer)(p.mmsg),
	)
	if action == nil {
		return fmt.Errorf("error adding action to poller")
	}
	// TODO: action is allocated by C and needs to be freed.
	_ = action
	return nil
}

func (p *Receiver) Run() {
	p.wg.Add(1)
	go func() {
		defer p.wg.Done()
		_ = C.poller_run(unsafe.Pointer(p.poller))
	}()
}

func (p *Receiver) Close() {
	// TODO: poller_stop should block until the poller actually stops
	C.poller_stop(p.poller)
	p.wg.Wait()
	C.poller_destroy(&p.poller)
	DestroyMultiMsg(&p.mmsg)
}
