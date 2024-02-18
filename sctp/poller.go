package sctp

// #cgo CFLAGS: -g -Wall -I../core/src
// #cgo LDFLAGS: -L${SRCDIR}/core/lib -lsctpcore
// #include <stdlib.h>
// #include "adapter.h"
// extern void pollerAction(int fd, void* args);
// static poller_action_t *poller_add_external(poller_t *poller, int fd, void *args) {
//     poller_action_t *action = calloc(1, sizeof(poller_action_t));
//     action->fd = fd;
//     action->cb = pollerAction;
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
// If we want to handle messages received from various sockets
// monitored by this poller, then we need to register a go function
// as an action callback.
// This approach might have some problems:
// - thundering herd
// - thread starvation
// - if the callback functions block, they will block the poller.
type Poller struct {
	wg     sync.WaitGroup
	poller *C.struct_poller
	mmsg   *C.struct_mmsg
	// TODO: store an array for added actions. free them on Close().
}

func NewPoller(timeout int) *Poller {
	mmsg := CreateMultiMsg(10, 9216)
	return &Poller{
		wg:     sync.WaitGroup{},
		poller: C.poller_create(C.int(timeout)),
		mmsg:   mmsg,
	}
}

// https://stackoverflow.com/questions/6125683/call-go-functions-from-c

//export pollerAction
func pollerAction(fd C.int, args unsafe.Pointer) {
	mmsg := (*C.struct_mmsg)(args)
	// TODO: can I propagate the main context up to here?
	// I tried to wrap the context in the args, but cgo complained
	// about passing Go pointers to Go pointers.
	// panic: runtime error: cgo argument has Go pointer to Go pointer
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	numMsg, err := RecvMultiMsg(ctx, int(fd), mmsg)
	if err != nil {
		fmt.Printf("error reading message: %s", err.Error())
		return
	}
	mmsgit := GetMultiMsgIterator(mmsg)
	bytes := mmsgit.Next()
	for numMsg > 0 {
		// TODO: this is example code; printf is commented out to not
		// pollute stdout.
		//fmt.Printf("received buf: %s, len: %d\n",
		//	string(bytes), len(bytes))
		bytes = mmsgit.Next()
		_ = bytes
		numMsg--
	}
}

func (p *Poller) Add(fd int) {
	action := C.poller_add_external(
		p.poller,
		C.int(fd),
		(unsafe.Pointer)(p.mmsg),
	)
	// TODO: action is allocated by C and needs to be freed.
	_ = action
}

func (p *Poller) Run() {
	p.wg.Add(1)
	go func() {
		defer p.wg.Done()
		_ = C.poller_run(unsafe.Pointer(p.poller))
	}()
}

func (p *Poller) Close() {
	// TODO: poller_stop should block until the poller actually stops
	C.poller_stop(p.poller)
	p.wg.Wait()
	C.poller_destroy(&p.poller)
	DestroyMultiMsg(&p.mmsg)
}
