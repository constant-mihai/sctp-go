package sctp

// #cgo CFLAGS: -g -Wall -I${SRCDIR}/../core/src
// #cgo LDFLAGS: -L${SRCDIR}/../core/lib -lsctpcore
// #include <stdlib.h>
// #include <stdint.h>
// #include "adapter.h"
// extern void enqueueFD(int fd, uintptr_t args);
// static poller_action_t *poller_add_enqueuer(poller_t *poller, int fd, uintptr_t args) {
//     poller_action_t *action = calloc(1, sizeof(poller_action_t));
//     action->fd = fd;
//     action->cb = enqueueFD;
//     action->args = args;
//	   if (poller_add(poller, action)) {
//	      return NULL;
//	   }
//
//     return action;
// }
import "C"
import (
	"runtime/cgo"
	"sync"
	"unsafe"
)

// This is a wrapper structure for an epoll implemented in C.
//
// It delegates work to other threads.
// When the poller receives an event, it enqueues the FD in a shared
// queue. Worker threads started separately dequeue and read from
// the dequeued socket FD.
type Poller struct {
	wg     sync.WaitGroup
	poller *C.struct_poller
	// queue carries the FDs the poller produced. It is buffered, and the
	// producer never blocks on it; see enqueueFD.
	queue chan int
	// handle keeps the poller reachable from the C callback. cgo forbids
	// passing a Go pointer through C, so a cgo.Handle rides through the
	// action's args instead.
	handle cgo.Handle
	// TODO: store an array for added actions. free them on Close().
}

// DefaultQueueSize is the number of FDs a poller buffers before it starts
// dropping wakeups.
const DefaultQueueSize = 1024

func NewPoller(timeout int) *Poller {
	p := &Poller{
		wg:     sync.WaitGroup{},
		poller: C.poller_create(C.int(timeout)),
		queue:  make(chan int, DefaultQueueSize),
	}
	p.handle = cgo.NewHandle(p)
	return p
}

// Queue returns the channel on which the poller publishes readable FDs.
// Consumers range over it; it is closed by Close.
func (p *Poller) Queue() <-chan int {
	return p.queue
}

// https://stackoverflow.com/questions/6125683/call-go-functions-from-c

//export enqueueFD
func enqueueFD(fd C.int, args C.uintptr_t) {
	p := cgo.Handle(args).Value().(*Poller)
	// Non-blocking on purpose: this runs on the C epoll thread, so a
	// blocking send would stall dispatch once the queue filled up. Dropping
	// is safe because the registration is level-triggered — epoll reports
	// the fd again on the next wait.
	select {
	case p.queue <- int(fd):
	default:
	}
}

func (p *Poller) Add(fd int) {
	action := C.poller_add_enqueuer(
		p.poller,
		C.int(fd),
		C.uintptr_t(p.handle),
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
	// Only safe after the poller thread is gone: the callback resolves this
	// handle on every event.
	p.handle.Delete()
	close(p.queue)
}
