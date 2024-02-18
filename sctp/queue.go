package sctp

import (
	"container/list"
	"fmt"
	"sync"
)

type Queue struct {
	queue *list.List
	mutex sync.Mutex
}

func NewQueue() *Queue {
	return &Queue{
		queue: list.New(),
		mutex: sync.Mutex{},
	}
}

func (q *Queue) Push(fd int) {
	q.mutex.Lock()
	defer q.mutex.Unlock()
	q.queue.PushBack(fd)
}

func (q *Queue) Pop() (int, error) {
	q.mutex.Lock()
	defer q.mutex.Unlock()
	e := q.queue.Front()
	if e == nil {
		return -1, nil
	}
	fd, ok := q.queue.Remove(e).(int)
	if !ok {
		return -1, fmt.Errorf("error converting type")
	}
	return fd, nil
}

func (q *Queue) Len() int {
	q.mutex.Lock()
	defer q.mutex.Unlock()
	return q.queue.Len()
}
