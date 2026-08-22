package sctp

import (
	"context"
	"sync"
	"testing"
	"time"
)

func TestPoller(t *testing.T) {
	server := NewSctpServer("0.0.0.0", 20304)
	client := NewSctpClient("127.0.0.1", 40302)
	poller := NewPoller(100 /* timeout in milliseconds */)

	poller.Add(server.FD())
	poller.Run()

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	// Counted rather than printed: the send loop below runs flat out for
	// three seconds, so one line per message buries the test output.
	var (
		wg             sync.WaitGroup
		nreceived      int
		nbytes         int
		nnotifications int
	)
	wg.Add(1)
	go func(ctx context.Context) {
		defer wg.Done()
		mmsg := CreateMultiMsg(10, 9216)
		defer DestroyMultiMsg(&mmsg)
		for {
			// Blocking handoff: the poller closes the queue on Close, and
			// the context bounds the wait if it never produces anything.
			var fd int
			select {
			case <-ctx.Done():
				return
			case v, ok := <-poller.Queue():
				if !ok {
					return
				}
				fd = v
			}

			// Scoped so the deferred Rearm covers the read error path too:
			// an fd that is not re-armed is never reported again, so missing
			// it once takes the socket out of the poller permanently.
			func() {
				// After processing, not after reading: re-arming earlier
				// would let another worker take the next batch off this
				// socket and overtake this one.
				defer func() {
					if err := poller.Rearm(fd); err != nil {
						t.Logf("error rearming fd: %s", err)
					}
				}()

				// The fd is only known to have been readable when the poller
				// enqueued it, so bound the read instead of spinning on EAGAIN
				// with a background context.
				readCtx, readCancel := context.WithTimeout(ctx, 100*time.Millisecond)
				numMsg, err := RecvMultiMsg(readCtx, int(fd), mmsg)
				readCancel()
				if err != nil {
					t.Logf("error reading message: %s", err.Error())
					return
				}

				mmsgit := GetMultiMsgIterator(mmsg)
				for i := 0; i < numMsg; i++ {
					msg := mmsgit.Next()
					if msg.IsNotification {
						nnotifications++
						t.Logf("sctp notification: %s", msg)
						continue
					}
					nreceived++
					nbytes += len(msg.Bytes)
				}
			}()
		}
	}(ctx)

	defer cancel()
	for ctx.Err() == nil {
		_, err := SendMsg(
			ctx,
			client.FD(),
			"127.0.0.1", 20304, []byte("go test"),
		)
		if err != nil {
			t.Errorf("Got error on send: %s", err.Error())
		}
	}

	// Wait before reading the counters: the consumer goroutine owns them.
	wg.Wait()
	t.Logf("received %d messages, %d bytes, %d notifications",
		nreceived, nbytes, nnotifications)

	poller.Close()
}
