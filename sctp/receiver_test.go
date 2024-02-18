package sctp

import (
	"context"
	"testing"
	"time"
)

func TestReceiver(t *testing.T) {
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

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()
	for ctx.Err() == nil {
		_, err := SendMsg(
			ctx,
			client.FD(),
			"127.0.0.1", 10203, []byte("go test"),
		)
		if err != nil {
			t.Errorf("Got error on send: %s", err.Error())
		}
	}

	for _, r := range receiver {
		r.Close()
	}
}
