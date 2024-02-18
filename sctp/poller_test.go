package sctp

import (
	"context"
	"testing"
	"time"
)

func TestPoller(t *testing.T) {
	server := NewSctpServer("0.0.0.0", 10203)
	client := NewSctpClient("127.0.0.1", 30201)
	pollers := []*Poller{
		NewPoller(100 /* timeout in milliseconds */),
		NewPoller(100 /* timeout in milliseconds */),
	}

	for _, p := range pollers {
		p.Add(server.FD())
		p.Run()
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

	for _, p := range pollers {
		p.Close()
	}
}
