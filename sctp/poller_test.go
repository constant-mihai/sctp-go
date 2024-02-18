package sctp

import (
	"context"
	"fmt"
	"testing"
	"time"
)

func TestPoller(t *testing.T) {
	server := NewSctpServer("0.0.0.0", 20304)
	client := NewSctpClient("127.0.0.1", 40302)
	PollerQueue = NewQueue()
	// TODO: this would be ideal, see comments in the poller.
	//poller := NewPoller(queue, 100 /* timeout in milliseconds */)
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

			numMsg, err := RecvMultiMsg(context.Background(), int(fd), mmsg)
			if err != nil {
				fmt.Printf("error reading message: %s", err.Error())
				return
			}

			mmsgit := GetMultiMsgIterator(mmsg)
			bytes := mmsgit.Next()
			for numMsg > 0 {
				// TODO: this doesn't print anything. Why?
				fmt.Printf("received buf: %s, len: %d\n",
					string(bytes), len(bytes))
				bytes = mmsgit.Next()
				_ = bytes
				numMsg--
			}
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

	poller.Close()
}
