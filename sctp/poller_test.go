package sctp

import (
	"context"
	"testing"
	"time"
)

func TestPoller(t *testing.T) {
	server := NewSctpServer("0.0.0.0", 10203)
	client := NewSctpClient("127.0.0.1", 30201)
	poller := NewPoller(100 /* timeout in milliseconds */)
	poller.Add(server.FD())
	poller.Run()

	time.Sleep(200 * time.Millisecond)

	_, err := SendMsg(
		context.Background(),
		client.FD(),
		"127.0.0.1", 10203, []byte("go test"),
	)
	if err != nil {
		t.Errorf("Got error on send: %s", err.Error())
	}

	time.Sleep(3 * time.Second)
	poller.Close()
}
