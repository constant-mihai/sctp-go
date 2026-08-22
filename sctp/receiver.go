package sctp

// #cgo CFLAGS: -g -Wall -I${SRCDIR}/../core/src
// #cgo LDFLAGS: -L${SRCDIR}/../core/lib -lsctpcore
// #include <stdlib.h>
// #include <stdint.h>
// #include "adapter.h"
// extern void receiveMultiMessage(int fd, uintptr_t args);
// static poller_action_t *poller_add_receiver(poller_t *poller, int fd, uintptr_t args) {
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
	"runtime/cgo"
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
	// handle keeps the receiver reachable from the C callback: cgo forbids
	// passing a Go pointer through C, so a cgo.Handle rides through the
	// action's args instead.
	handle cgo.Handle
	// actions maps an FD to the C action registered for it. The registration
	// is EPOLLONESHOT, so the callback has to re-arm through the original
	// pointer once it has finished with the batch.
	mu      sync.RWMutex
	actions map[int]*C.poller_action_t
}

func NewReceiver(timeout int) *Receiver {
	r := &Receiver{
		wg:      sync.WaitGroup{},
		poller:  C.poller_create(C.int(timeout)),
		mmsg:    CreateMultiMsg(10, 9216),
		actions: make(map[int]*C.poller_action_t),
	}
	r.handle = cgo.NewHandle(r)
	return r
}

// https://stackoverflow.com/questions/6125683/call-go-functions-from-c

//export receiveMultiMessage
func receiveMultiMessage(fd C.int, args C.uintptr_t) {
	r := cgo.Handle(args).Value().(*Receiver)
	// EPOLLONESHOT disarmed this fd when it was reported. Deferred rather than
	// called at the end, so the error paths below re-arm too: skipping it once
	// takes the socket out of the poller for good.
	defer func() {
		if err := r.rearm(int(fd)); err != nil {
			fmt.Printf("error rearming fd: %s\n", err)
		}
	}()
	mmsg := r.mmsg
	// TODO: can I propagate the main context up to here?
	// The receiver itself now arrives through a cgo.Handle, so anything
	// hanging off it — a context, a channel — is reachable from here.
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	// TODO: is it ok to timeout? does it mean there's nothing to read?
	numMsg, err := RecvMultiMsg(ctx, int(fd), mmsg)
	if err != nil {
		fmt.Printf("error reading message: %s", err.Error())
		return
	}
	mmsgit := GetMultiMsgIterator(mmsg)
	for i := 0; i < numMsg; i++ {
		msg := mmsgit.Next()
		if msg.IsNotification {
			fmt.Printf("sctp notification: %s\n", msg)
			continue
		}
		// TODO: this is example code; Printf is commented out to not
		// pollute stdout.
		//fmt.Printf("received buf: %s, len: %d\n",
		//	msg, len(msg.Bytes))
	}
}

// TODO: what happens if the same FD is added multiple times?
func (p *Receiver) Add(fd int) error {
	action := C.poller_add_receiver(
		p.poller,
		C.int(fd),
		C.uintptr_t(p.handle),
	)
	if action == nil {
		return fmt.Errorf("error adding action to poller")
	}

	p.mu.Lock()
	defer p.mu.Unlock()
	p.actions[fd] = action
	return nil
}

// rearm re-enables an fd that EPOLLONESHOT disarmed when it was reported. The
// callback runs inline on the epoll thread and does its own reading, so it is
// the callback that owns the re-arm, at the point where it is done processing.
func (p *Receiver) rearm(fd int) error {
	p.mu.RLock()
	action, ok := p.actions[fd]
	p.mu.RUnlock()
	if !ok {
		return fmt.Errorf("fd %d is not registered with this receiver", fd)
	}

	if C.poller_rearm(p.poller, action) != 0 {
		return fmt.Errorf("error rearming fd %d", fd)
	}
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
	// Only safe after the poller thread is gone: the callback resolves this
	// handle on every event.
	p.handle.Delete()

	p.mu.Lock()
	for fd, action := range p.actions {
		C.free(unsafe.Pointer(action))
		delete(p.actions, fd)
	}
	p.mu.Unlock()

	DestroyMultiMsg(&p.mmsg)
}
