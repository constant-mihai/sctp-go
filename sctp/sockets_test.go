package sctp

import (
	"context"
	"errors"
	"fmt"
	"io"
	"sync"
	"testing"
	"time"
)

// TODO: figure this out:
// === RUN   TestSockets
// buf: �

// J, len: 20
// buf: go test, len: 7
// buf: , len: 0
// --- PASS: TestSockets (2.00s)

// TODO: for some reason src addr is always empty:
// received buf: go test, len: 7 from: <nil>:0
func TestSocketsSingleMessage(t *testing.T) {
	server := NewSctpServer("0.0.0.0", 12345)
	client := NewSctpClient("127.0.0.1", 54321)

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	messages := 0
	wg := sync.WaitGroup{}
	wg.Add(1)
	go func(ctx context.Context) {
		defer wg.Done()
		for ctx.Err() == nil {
			msg, ip, port, err := RecvMsg(ctx, server.FD())
			if errors.Is(err, io.EOF) {
				return
			}
			if err != nil {
				t.Errorf("error reading message: %s", err.Error())
			}
			if msg.IsNotification {
				fmt.Printf("sctp notification: %s\n", msg)
				continue
			}
			messages += len(msg.Bytes)
			fmt.Printf("received buf: %s, len: %d from: %s:%d\n",
				msg, len(msg.Bytes), ip.String(), port)
		}
	}(ctx)

	time.Sleep(200 * time.Millisecond)

	nsent, err := SendMsg(ctx, client.FD(), "127.0.0.1", 12345, []byte("go test"))
	if err != nil {
		t.Errorf("Got error on send: %s", err.Error())
	}

	wg.Wait()
	if messages < nsent {
		t.Errorf("Expected messages: %d to be >= nsent: %d", messages, nsent)
	}
}

func TestSocketsMultiMessage(t *testing.T) {
	server := NewSctpServer("0.0.0.0", 12346)
	client := NewSctpClient("127.0.0.1", 64321)

	mmsg := CreateMultiMsg(10, 9216)
	defer DestroyMultiMsg(&mmsg)

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	messages := 0
	wg := sync.WaitGroup{}
	wg.Add(1)
	go func(ctx context.Context) {
		defer wg.Done()
		for ctx.Err() == nil {
			numMsg, err := RecvMultiMsg(ctx, server.FD(), mmsg)
			if err != nil {
				t.Errorf("error reading message: %s", err.Error())
			}
			mmsgit := GetMultiMsgIterator(mmsg)
			for i := 0; i < numMsg; i++ {
				msg := mmsgit.Next()
				if msg.IsNotification {
					fmt.Printf("sctp notification: %s\n", msg)
					continue
				}
				messages += len(msg.Bytes)
				fmt.Printf("received buf: %s, len: %d\n",
					msg, len(msg.Bytes))
			}
		}
	}(ctx)

	time.Sleep(200 * time.Millisecond)

	nsent, err := SendMsg(ctx, client.FD(), "127.0.0.1", 12346, []byte("go test"))
	if err != nil {
		t.Errorf("Got error on send: %s", err.Error())
	}

	wg.Wait()
	if messages < nsent {
		t.Errorf("Expected messages: %d to be >= nsent: %d", messages, nsent)
	}
}

func TestSocketsNotifications(t *testing.T) {
	// TODO
}
