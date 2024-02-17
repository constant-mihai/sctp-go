package sctp

import (
	"context"
	"fmt"
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
			bytes, ip, port, err := server.RecvMsg(ctx)
			if err != nil {
				t.Errorf("error reading message: %s", err.Error())
			}
			messages += len(bytes)
			fmt.Printf("received buf: %s, len: %d from: %s:%d\n",
				string(bytes), len(bytes), ip.String(), port)
		}
	}(ctx)

	time.Sleep(200 * time.Millisecond)

	nsent, err := client.SendMsg(ctx, "127.0.0.1", 12345, []byte("go test"))
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
			numMsg, err := server.RecvMultiMsg(ctx, mmsg)
			if err != nil {
				t.Errorf("error reading message: %s", err.Error())
			}
			mmsgit := GetMultiMsgIterator(mmsg)
			bytes := mmsgit.Next()
			for numMsg > 0 {
				messages += len(bytes)
				fmt.Printf("received buf: %s, len: %d\n",
					string(bytes), len(bytes))
				bytes = mmsgit.Next()
				numMsg--
			}
		}
	}(ctx)

	time.Sleep(200 * time.Millisecond)

	nsent, err := client.SendMsg(ctx, "127.0.0.1", 12346, []byte("go test"))
	if err != nil {
		t.Errorf("Got error on send: %s", err.Error())
	}

	wg.Wait()
	if messages < nsent {
		t.Errorf("Expected messages: %d to be >= nsent: %d", messages, nsent)
	}
}
