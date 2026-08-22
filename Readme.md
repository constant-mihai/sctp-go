# Go wrapper for SCTP

This is an experimental library with the following objectives:
1. Learn CGO.
2. Offer an API that allows to create active/passive SCTP sockets. Go doesn't offer any SCTP support. The user would have to use syscalls for creating SCTP sockets and setting socket options.
3. Read and write SCTP messages in batches. Go doesn't offer an equivalent to `struct mmsghdr`, `recvmmsg`, `sendmmsg` for SCTP sockets.
[ReadBatch](https://pkg.go.dev/golang.org/x/net/ipv4#PacketConn.ReadBatch) and [WriteBatch](https://pkg.go.dev/golang.org/x/net/ipv4#PacketConn.WriteBatch)
only work on a `PacketConn`.
4. Use epoll and distribute work to multiple workers when fds are ready to read/write. 


- [georgeyanev/go-sctp](https://github.com/georgeyanev/go-sctp)
presents one example of how to hook up SCTP Socket fds to the netpoller. It opens a non-blocking IPPROTO_SCTP socket, sets options, bindx, listen,
and then uses os.NewFile to hook to the netpoller.
- [ishidawataru/sctp](https://github.com/ishidawataru/sctp) ignores the netpoller in favour of using syscalls. It
opens a blocking socket, bindx, listen, then keeps the bare int fd, so every Recvmsg/Accept blocks an OS thread.
- this repo use CGO and writes it's own epoll for monitoring fds and as well container structures for mmmsg. This is for learning purposes. CGO isn't really needed, everything here can be achieved using GO Syscalls.

## Receiver and Poller

The repo experiments with two design choices:
- Receiver: epoll thread wakes on EPOLLIN, calls a Go callback in-thread; the callback recvmmsgs into a shared container and iterates messages.
- Poller: epoll thread wakes, pushes the fd onto a buffered channel; separate goroutines receive, recvmmsg, and process.

## Example usage
There are two ways to use the library. Both of them can be explored in `receiver_test.go` and `poller_test.go`.
```go
server := NewSctpServer("0.0.0.0", 10203)
client := NewSctpClient("127.0.0.1", 30201)
receivers := []*Receiver{
    NewReceiver(100 /* timeout in milliseconds */),
    NewReceiver(100 /* timeout in milliseconds */),
}

for _, r := range receivers {
    if err := r.Add(server.FD()); err != nil {
        log.Fatalf("error adding fd to receiver: %s", err)
    }
    r.Run()
}

// Received messages are handled by the callback running on the epoll
// thread, so there is nothing to read here; just keep sending.
ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
defer cancel()
for ctx.Err() == nil {
    if _, err := SendMsg(ctx, client.FD(), "127.0.0.1", 10203, []byte("hello")); err != nil {
        log.Fatalf("error sending: %s", err)
    }
}

for _, r := range receivers {
    r.Close()
}
```

The `poller` is intended to be used as a producer for a queue. It produces FDs. The user can then start goroutines which read from these FDs when they appear in the queue.

FDs are registered with `EPOLLONESHOT`, so an FD is produced at most once at a time and
whichever worker takes it owns that socket until it calls `Rearm`. Any worker serves any
FD; the kernel only guarantees that no two serve the same one concurrently, which is what
keeps SCTP's per-stream ordering intact. **An FD that is never re-armed is never produced
again**, so `Rearm` belongs in a `defer` that covers the error paths too.
```go
server := NewSctpServer("0.0.0.0", 20304)
client := NewSctpClient("127.0.0.1", 40302)
poller := NewPoller(100 /* timeout in milliseconds */)

poller.Add(server.FD())
poller.Run()

ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
defer cancel()

// Consumer: drains the FDs produced by the poller. Start as many of
// these as you want workers.
go func(ctx context.Context) {
    mmsg := CreateMultiMsg(10, 9216)
    defer DestroyMultiMsg(&mmsg)
    // Queue() is closed by Close, so the range ends with the poller.
    for fd := range poller.Queue() {
        func() {
            // Deferred so every path re-arms, including the read error
            // below. After processing, never between read and process:
            // re-arming early lets another worker overtake this one.
            defer poller.Rearm(fd)

            // Bound the read: RecvMultiMsg retries on EAGAIN until its
            // context expires.
            readCtx, readCancel := context.WithTimeout(ctx, 100*time.Millisecond)
            numMsg, err := RecvMultiMsg(readCtx, fd, mmsg)
            readCancel()
            if err != nil {
                log.Printf("error reading message: %s", err)
                return
            }

            mmsgit := GetMultiMsgIterator(mmsg)
            for i := 0; i < numMsg; i++ {
                msg := mmsgit.Next()
                // Notifications arrive on the same socket as user data;
                // printing one as a string gives you the decoded event.
                if msg.IsNotification {
                    fmt.Printf("sctp notification: %s\n", msg)
                    continue
                }
                fmt.Printf("received %d bytes: %s\n", len(msg.Bytes), msg)
            }
        }()
    }
}(ctx)

for ctx.Err() == nil {
    if _, err := SendMsg(ctx, client.FD(), "127.0.0.1", 20304, []byte("hello")); err != nil {
        log.Fatalf("error sending: %s", err)
    }
}

poller.Close()
```

## Install
Dependencies: a C toolchain, the Go toolchain, and the lksctp headers
(`libsctp-dev` on Debian/Ubuntu, `lksctp-tools-devel` on Fedora). The C code links
against `-lsctp` and `-lpthread`.
`TODO: a build on a fresh machine hasn't been verified beyond this list.`

Everything is driven from the root Makefile:
```
make        # compiles core/ and links core/lib/libsctpcore.so
make test   # builds the library if needed, then runs go test -v ./sctp
make ctest  # runs the C test suite
make clean
```
`make test` points `LD_LIBRARY_PATH` at `core/lib`, so the shared object does not
have to be installed system-wide to run the tests. If you do want it on the
default loader path:
```
sudo make install   # copies the .so to /usr/local/lib and runs ldconfig
```


## Learnings

### Receiver Pool 

The `receiver` is intended to be initialized as a pool of epoll threads. On these threads we can register FDs for monitoring.
When there is an EPOLLIN event, the epoll thread runs the callback associated with the FD. There might be some problems with this approach like:
- the pool of epoll threads buys nothing: with one-to-many SCTP every association
arrives on a single fd, so there is nothing to shard across threads, and a wakeup hands
the whole backlog to whichever thread the kernel picked.
- thread blockage, the callback runs inline in the event loop, so anything it waits
on — a mutex, a GC assist — delays the next `epoll_wait` on that thread.
- every event crosses from C into Go through a cgo callback, on a thread the runtime
has to attach; the poller mode pays this once per wakeup instead of once per fd.

### Poller

The `poller` produces fds and lets ordinary goroutines do the reading, which keeps Go
work on Go threads. It publishes them on a buffered channel exposed by `Queue()`; the
send is non-blocking, because it runs on the C epoll thread and must not stall dispatch
when consumers fall behind, and a dropped wakeup is harmless under level-triggered
epoll. The channel can be owned by the `Poller` rather than being a package global
because the action carries a `runtime/cgo.Handle` to it through a `uintptr_t` — cgo
will not let a Go pointer be stored in C memory, and a handle is an integer.

Findings, in the order they matter:

1. Concurrent readers on one socket lose per-stream ordering. The kernel serializes on
the receive queue, so N workers calling `recvmmsg` on the same fd do get disjoint
messages — but two of them can take consecutive messages of the same stream and finish
in the opposite order. Hashing `(assoc_id, stream)` *after* the read does not fix this:
the order in which two threads hand off to the hashed worker is itself a race. Order is
only recoverable if the point where sequence is established is single-threaded.
2. `EPOLLONESHOT` is what makes that single-threaded, and it is what the poller now
registers with (`core/src/poller.c`). The fd is disarmed the moment it is reported, so
exactly one worker holds a socket until it calls `Rearm`. This does not pin an
association to a worker — any worker takes any fd — it only guarantees no two serve the
same one at once. `EPOLLEXCLUSIVE` was the wrong tool here: it suppresses thundering-herd
wakeups across multiple waiters on one fd, and there is a single dispatcher thread.
3. Re-arm after processing, not after reading. Re-arming between the `recvmmsg` and the
work lets the next worker read the following batch and finish ahead of the current one,
which puts the messages back out of order — the exact failure `EPOLLONESHOT` was added
to prevent.
4. Dropping a wakeup is now fatal, not free. Under level-triggered registration a dropped
fd was re-reported on the next wait; under `EPOLLONESHOT` it stays disarmed and that
socket goes silent permanently. `enqueueFD` blocks on a full queue for this reason, and
`Close` closes a `done` channel to release a producer stuck there. Blocking the epoll
thread only delays the fds that are still armed; it cannot lose them.
5. Hashing is only needed for parallelism *inside* one association. With one fd per peer
and one worker holding it at a time, every ordering guarantee SCTP makes already holds.
Splitting streams of a single association across workers is the case that needs the hash
key — and the data for it is currently discarded: neither `mmsg_create` nor `RecvMsg`
sets `msg_control`/`msg_controllen`, so `sctp_sndrcvinfo` never arrives despite
`sctp_data_io_event` being subscribed. Every read sets `MSG_CTRUNC`. The modern
equivalent is `SCTP_RECVRCVINFO` → `struct sctp_rcvinfo` (RFC 6458).
6. Producing an fd still costs a C-to-Go transition. `enqueueFD` is Go code entering
from a thread the runtime has to hand a P to, so it waits when every P is busy and
stalls outright during a stop-the-world pause. The poller pays this once per wakeup
rather than once per message, which is the whole of its advantage over the receiver —
it does not escape it.

### Pinning work to cores

Go offers exactly one mechanism for this — `runtime.LockOSThread` to bind a goroutine
to a thread, then `unix.SchedSetaffinity` to bind that thread to a core. The lock is
what makes the netpoller's wakeup land on a known thread; the affinity call is what
stops the kernel migrating that thread and dragging the flow's state into a cold cache.
What differs between the three libraries is not the mechanism but what there is to pin.

`ishidawataru/sctp` and `go-sctp` are one-to-one: an association is a socket, so
pinning its goroutine pins the whole flow, and per-stream ordering is preserved by
construction rather than by hashing. `ishidawataru/sctp` goes furthest —
its read blocks on that same thread, so discovery, read and processing all happen on
the pinned core, at the cost of a thread per association. `go-sctp` pays one cross-core
wakeup per event instead, because readiness is discovered by whichever thread the
runtime happens to be polling on.

For this repo pinning can happen only after a message has been read and hashed.

### Do the messages have to be copied out of C?

TODO; Disclaimer: this is llm generated and left here more as a reminder to myself on what can
be explored next. According to the LLM the C.GoBytes mem copy can be avoided.

No, and this was the most useful thing to learn. Today every message is copied twice
after the kernel writes it: once by `recvmmsg` into the buffers `mmsg_create`
allocated, and once by `C.GoBytes` into a fresh Go slice. `GoBytes` is a `mallocgc`
plus a `memmove`, so a batch of ten messages costs ten heap allocations — which
cancels much of the point of batching, since `recvmmsg` was supposed to amortize one
syscall and one cgo transition across the whole batch.

Two ways out. The cheap one is to borrow rather than copy: `unsafe.Slice` over
`iov_base` yields a Go slice aliasing the C buffer with no allocation, valid only
until the next `recvmmsg` overwrites it — which suits a callback API that promises not
to retain the bytes. The thorough one is to let the kernel scatter directly into Go
memory: allocate the receive buffers as Go `[]byte`, pin them with `runtime.Pinner`,
and store those pointers in the `iovec`s. Pinned pointers may legally be stored in C
memory, so `recvmmsg` writes straight into the Go heap and the second copy disappears
entirely. That also answers the original complaint that there was no way to work on
these buffers directly in Go: there is, it just needs pinning.
