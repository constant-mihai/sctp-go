# Go wrapper for SCTP

This is an experimental library which is looking to identify solutions for the following problems:
1. Go doesn't offer any SCTP support. The user would have to use syscalls for creating SCTP sockets and setting socket options.
2. Go doesn't offer an equivalent to `struct mmsghdr`, `recvmmsg`, `sendmmsg` for SCTP sockets.
[ReadBatch](https://pkg.go.dev/golang.org/x/net/ipv4#PacketConn.ReadBatch) and [WriteBatch](https://pkg.go.dev/golang.org/x/net/ipv4#PacketConn.WriteBatch)
only work on a `PacketConn`.
3. How can I use non-blocking operations when reading/writing into multiple SCTP sockets.


- [georgeyanev/go-sctp](https://github.com/georgeyanev/go-sctp)
presents one example of how to hook up SCTP Socket fds to the netpoller. It opens a non-blocking IPPROTO_SCTP socket, sets options, bindx, listen,
and then uses os.NewFile to hook to the netpoller.
- [ishidawataru/sctp](https://github.com/ishidawataru/sctp) ignores the netpoller in favour of using syscalls. It
opens a blocking socket, bindx, listen, then keeps the bare int fd, so every Recvmsg/Accept blocks an OS thread.
- this repo use CGO and writes it's own epoll for monitoring fds and as well container structures for mmmsg.

## Example usage
There are two ways to use the library. Both of them can be explored in `receiver_test.go` and `poller_test.go`.

The `receiver` is intended to be initialized as a pool of epoll threads. On these threads we can register FDs for monitoring.
When there is an EPOLLIN event, the epoll thread runs the callback associated with the FD. There might be some problems with this approach like:
- thundering herd, all epoll threads will wake up when an FD sees an event.
- thread starvation, the fastest epoll thread will do all the work.
- thread blockage, if the callback functions block, they will block the poller.
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
```go
server := NewSctpServer("0.0.0.0", 20304)
client := NewSctpClient("127.0.0.1", 40302)
PollerQueue = NewQueue()
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
    for ctx.Err() == nil {
        // get something from the queue
        fd, err := PollerQueue.Pop()
        if err != nil {
            log.Printf("error popping from the queue: %s", err)
            continue
        }
        if fd < 0 { // queue is empty
            time.Sleep(100 * time.Millisecond)
            continue
        }

        // Bound the read: RecvMultiMsg retries on EAGAIN until its
        // context expires.
        readCtx, readCancel := context.WithTimeout(ctx, 100*time.Millisecond)
        numMsg, err := RecvMultiMsg(readCtx, fd, mmsg)
        readCancel()
        if err != nil {
            log.Printf("error reading message: %s", err)
            continue
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


## Performance
`TODO: I haven't benchmarked it yet`
The C part will do bulk copy of the messages, minimizing context switches between the kernel and user space.
The bad part is that I haven't figured out a way to work on these buffers directly in Go, so I copy them over once again using C.GoBytes.
