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
	"fmt"
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
//
// Registration is EPOLLONESHOT, so an FD appears in the queue at most once at
// a time: whichever worker takes it owns that socket until it calls Rearm. Any
// worker can serve any FD — the kernel only guarantees that no two serve the
// same one concurrently, which is what preserves SCTP's per-stream ordering.
type Poller struct {
	wg     sync.WaitGroup
	poller *C.struct_poller
	// queue carries the FDs the poller produced. Under EPOLLONESHOT a dropped
	// FD is never re-reported, so the producer must not drop; see enqueueFD.
	queue chan int
	// done is closed by Close to release a producer blocked on a full queue.
	done chan struct{}
	// handle keeps the poller reachable from the C callback. cgo forbids
	// passing a Go pointer through C, so a cgo.Handle rides through the
	// action's args instead.
	handle cgo.Handle
	// actions maps an FD to the C action registered for it. Rearm needs the
	// original pointer, because it is what epoll hands back in data.ptr.
	mu      sync.RWMutex
	actions map[int]*C.poller_action_t
}

// DefaultQueueSize is the number of FDs a poller buffers before the epoll
// thread blocks waiting for a consumer.
const DefaultQueueSize = 1024

func NewPoller(timeout int) *Poller {
	p := &Poller{
		wg:      sync.WaitGroup{},
		poller:  C.poller_create(C.int(timeout)),
		queue:   make(chan int, DefaultQueueSize),
		done:    make(chan struct{}),
		actions: make(map[int]*C.poller_action_t),
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
	// Blocking on purpose. This runs on the C epoll thread, so a full queue
	// stalls dispatch — but under EPOLLONESHOT the alternative is worse: a
	// dropped FD stays disarmed and that socket goes silent permanently.
	// Stalling only delays the FDs that are still armed; they are reported as
	// soon as the loop gets back to epoll_wait. Close releases us via done.
	select {
	case p.queue <- int(fd):
	case <-p.done:
	}
}

func (p *Poller) Add(fd int) error {
	action := C.poller_add_enqueuer(
		p.poller,
		C.int(fd),
		C.uintptr_t(p.handle),
	)
	if action == nil {
		return fmt.Errorf("error adding action to poller for fd %d", fd)
	}

	p.mu.Lock()
	defer p.mu.Unlock()
	p.actions[fd] = action
	return nil
}

// Rearm re-enables an FD taken from Queue. EPOLLONESHOT disarmed it when it was
// reported, so it is not reported again until this is called, and an FD that is
// never re-armed is never served again.
//
// Call it after the batch has been processed, not right after reading it:
// re-arming early lets another worker read the next batch from the same socket
// and finish ahead of this one, which puts the messages back out of order.
func (p *Poller) Rearm(fd int) error {
	p.mu.RLock()
	action, ok := p.actions[fd]
	p.mu.RUnlock()
	if !ok {
		return fmt.Errorf("fd %d is not registered with this poller", fd)
	}

	if C.poller_rearm(p.poller, action) != 0 {
		return fmt.Errorf("error rearming fd %d", fd)
	}
	return nil
}

func (p *Poller) Run() {
	p.wg.Add(1)
	go func() {
		defer p.wg.Done()
		_ = C.poller_run(unsafe.Pointer(p.poller))
	}()
}

func (p *Poller) Close() {
	// First, so that a producer blocked on a full queue can return and let the
	// epoll loop notice it has been stopped.
	close(p.done)
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

	close(p.queue)
}
