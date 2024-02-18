# Go wrapper for SCTP

This is an experimental library which is looking to identify solutions for the following problems:
1. Go doesn't offer any SCTP primitives. The user would have to use syscalls for creating SCTP sockets and setting socket options.
2. Go doesn't offer an equivalent to `struct mmsghdr`. (Maybe because `struct mmsghdr` is linux specific?)
3. Go doesn't offer access to the netpoller. For UDP and TCP it offers only high level instructions.
In the case of UDP one can read sequentially from the socket, for TCP a new goroutine should start for each `Accept()`.

The workaround for all three problems was to create a wrapper on top of a C library.
Point 1. can be solved as well via Go wrappers for syscall.
The reason for the using C code was because I needed something to test the C implementation of the `struct mmsghdr` container and epoll.
For point 2. there is some C code which helps create and manage a container for `struct mmsghdr`.
Lastly, for point 3., there is an epoll implementation into which the Go code can hook actions.

Send and receive are non-blocking.

## Example usage
There are two ways to use the library. Both of them can be explored in `receiver_test.go` and `poller_test.go`.

The `receiver` is intended to be initialized as a pool of epoll threads. On these threads we can register FDs for monitoring.
When there is an EPOLLIN event, the epoll thread runs the callback associated with the FD. There might be some problems with this approach like:
- thundering herd, all epoll threads will wake up when an FD sees an event.
- thread starvation, the fastest epoll thread will do all the work.
- thread blockage, if the callback functions block, they will block the poller.
```
server := NewSctpServer("0.0.0.0", 10203)
client := NewSctpClient("127.0.0.1", 30201)
receiver := []*Receiver{
    NewReceiver(100 /* timeout in milliseconds */),
    NewReceiver(100 /* timeout in milliseconds */),
}

for _, r := range receiver {
    if err := r.Add(server.FD()); err != nil {
        t.Errorf("Error adding fd to receiver: %s\n", err.Error())
    }
    r.Run()
}

for _, r := range receiver {
    r.Close()
}
```

The `poller` is intended to be used as a producer for a queue. It produces FDs. The user can then start goroutines which read from these FDs when they appear in the queue.
```
server := NewSctpServer("0.0.0.0", 20304)
client := NewSctpClient("127.0.0.1", 40302)
PollerQueue = NewQueue()
poller := NewPoller(100 /* timeout in milliseconds */)

poller.Add(server.FD())
poller.Run()

ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
defer cancel()
go func(ctx context.Context) {
    mmsg := CreateMultiMsg(10, 9216)
    defer DestroyMultiMsg(&mmsg)
    for ctx.Err() != nil {
        // get something fom the queue
        fd, err := PollerQueue.Pop()
        if err != nil {
            t.Errorf("error popping from the queue")
        }
        if fd < 0 {
            time.Sleep(100 * time.Millisecond)
            continue
        }
        ...
        }
    }
}(ctx)

poller.Close()
```

## Install
`TODO: I haven't tested yet a fresh build, so I cannot recommend deps installation.`
To test the library:
```
cd core/ && make shared && sudo make install && cd ../
go test -v ./sctp
```


## Performance
`TODO: I haven't benchmarked it yet`
The C part will do bulk copy of the messages, minimizing context switches between the kernel and user space.
The bad part is that I haven't figured out a way to work on these buffers directly in Go, so I copy them over once again using C.GoBytes.
