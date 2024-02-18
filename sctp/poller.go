package sctp

// #cgo CFLAGS: -g -Wall -I../core/src
// #cgo LDFLAGS: -L${SRCDIR}/core/lib -lsctpcore
// #include <stdlib.h>
// #include "adapter.h"
// extern void enqueueFD(int fd, void* args);
// static poller_action_t *poller_add_enqueuer(poller_t *poller, int fd, void *args) {
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
	"sync"
	"unsafe"
)

// TODO: I would prefer for this to not be a global, but I don't know yet
// how to pass it to the C poller callback. It has to be passed as a void
// pointer to the C code, which will then send it back to Go. See the
// comments on enqueueFD.
var PollerQueue *Queue

// This is a wrapper structure for an epoll implemented in C.
//
// It delegates work to other threads.
// When the poller receives an event, it enqueues the FD in a shared
// queue. Worker threads started separately dequeue and read from
// the dequeued socket FD.
type Poller struct {
	wg     sync.WaitGroup
	poller *C.struct_poller
}

func NewPoller(timeout int) *Poller {
	return &Poller{
		wg:     sync.WaitGroup{},
		poller: C.poller_create(C.int(timeout)),
	}
}

// https://stackoverflow.com/questions/6125683/call-go-functions-from-c

//export enqueueFD
func enqueueFD(fd C.int, args unsafe.Pointer) {
	PollerQueue.Push(int(fd))
}

// TODO: this doesn't work due to:
// panic: runtime error: cgo argument has Go pointer to Go pointer
//func enqueueFD(fd C.int, args unsafe.Pointer) {
//	queue := (*Queue)(args)
//	queue.Push(int(fd))
//}

func (p *Poller) Add(fd int) {
	action := C.poller_add_enqueuer(
		p.poller,
		C.int(fd),
		// TODO: see comments above
		//(unsafe.Pointer)(p.queue),
		(unsafe.Pointer)(nil),
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
}
