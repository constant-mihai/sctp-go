package sctp

// #cgo CFLAGS: -g -Wall -I${SRCDIR}/../core/src
// #cgo LDFLAGS: -L${SRCDIR}/../core/lib -lsctpcore
// #include <stdlib.h>
// #include "adapter.h"
import "C"
import (
	"context"
	"fmt"
	"io"
	"net"
	"syscall"
	"unsafe"
)

// Wrapper object for the C implementation.
// Go docs say:
// When the Go tool sees that one or more Go files use the special import "C", it will look for other
// non-Go files in the directory and compile them as part of the Go package. Any .c, .s, .S
// or .sx files will be compiled with the C compiler. Any .cc, .cpp, or .cxx files
// will be compiled with the C++ compiler. Any .f, .F, .for or .f90 files will be
// compiled with the fortran compiler. Any .h, .hh, .hpp, or .hxx files will not be compiled
// separately, but, if these header files are changed, the package (including its non-Go source files) will
// be recompiled. Note that changes to files in other directories do not cause the package to
// be recompiled, so all non-Go source code for the package should be stored in the package
// directory, not in subdirectories. The default C and C++ compilers may be changed by the CC
// and CXX environment variables, respectively; those environment variables may include command line options.
type Socket struct {
	fd int
}

func NewSctpServer(host string, port int) *Socket {
	socket := Socket{}
	fd := C.DefaultSctpServer(C.CString(host), C.uint16_t(port))
	socket.fd = int(fd)
	return &socket
}

func NewSctpClient(host string, port int) *Socket {
	socket := Socket{}
	fd := C.DefaultSctpClient(C.CString(host), C.uint16_t(port))
	socket.fd = int(fd)
	return &socket
}

func (s *Socket) FD() int {
	return s.fd
}

func SendMsg(ctx context.Context, fd int, dst string, port int, bytes []byte) (int, error) {
	// Go []byte slice to C array
	// The C array is allocated in the C heap using malloc.
	// It is the caller's responsibility to arrange for it to be
	// freed, such as by calling C.free (be sure to include stdlib.h
	// if C.free is needed).
	cBufBytes := C.CBytes(bytes)
	defer C.free(cBufBytes)
	for ctx.Err() == nil {
		nsent, err := C.SendMsg(
			C.int(fd),
			C.CString(dst),
			C.int(port),
			(*C.char)(cBufBytes),
			C.int(len(bytes)))
		switch {
		case nsent < 0:
			if err == nil {
				err = fmt.Errorf("unknown error while sending message")
				return int(nsent), err
			}
			errno := err.(syscall.Errno)
			if errno == C.EAGAIN || errno == C.EWOULDBLOCK {
				continue
			}
			err = fmt.Errorf("error sending message: %w", errno)
			return int(nsent), err
		case nsent == 0:
			// TODO: when can this happen?
		default:
			return int(nsent), err
		}
	}
	return 0, nil
}

func SendMultiMsg(fd int, dst string, port int, mmsg *C.struct_mmsg_t) int {
	//TODO
	return 0
}

// TODO: passing a context is not the idiomatic way for I/O ops in go.
// A SetDeadline() function is used instead.
// I like the ctx pattern better. I should understand,
// why the SetDeadline() pattern is preferred. If there's good reason for
// it, then I should switch.
// TODO: should this be blocking instead?
func RecvMsg(ctx context.Context, fd int) (msg Message, src net.IP, port int, err error) {
	cBufBytes := C.malloc(C.sizeof_char * 9216)
	defer C.free(cBufBytes)

	cSrcBytes := C.malloc(C.sizeof_char * 100)
	defer C.free(cSrcBytes)
	saddr := C.struct_sockaddr{}
	var flags C.int
LOOP:
	for ctx.Err() == nil {
		// The kernel reads saddr_len as the size of the address buffer and
		// writes back the size it filled in, so it has to be reset before
		// every call. Left at 0, no address is copied at all.
		saddr_len := C.uint(unsafe.Sizeof(saddr))
		nread, readErr := C.RecvMsg(
			C.int(fd),
			(*C.char)(cBufBytes),
			9216, /* buf len */
			&saddr, &saddr_len, &flags,
		)
		switch {
		case nread < 0:
			if readErr == nil {
				err = fmt.Errorf("unknown error while receiving message")
				return
			}
			errno := readErr.(syscall.Errno)
			if errno == C.EAGAIN || errno == C.EWOULDBLOCK {
				continue
			}
			err = fmt.Errorf("error reading message: %w", errno)
			return
		case nread == 0:
			// SCTP forbids zero-length user data, so a 0-byte read is not
			// an empty message: the association ended.
			err = io.EOF
			return
		default:
			msg = Message{
				Bytes:          C.GoBytes(cBufBytes, nread),
				IsNotification: flags&C.MSG_NOTIFICATION != 0,
			}
			ret, getAddrErr := C.GetAddress(
				&saddr,
				(*C.char)(cSrcBytes),
				100, /* cSrcBytes len */
				(*C.uint16_t)(unsafe.Pointer(&port)),
			)
			if ret < 0 {
				if getAddrErr != nil {
					errno := getAddrErr.(syscall.Errno)
					err = fmt.Errorf("error getting address: %w", errno)
				}
			} else {
				// GoString stops at the NUL; GoBytes would hand ParseIP the
				// whole padded buffer, which never parses.
				src = net.ParseIP(C.GoString((*C.char)(cSrcBytes)))
			}
			break LOOP
		}
	}

	return msg, src, port, err
}

// TODO: passing a context is not the idiomatic way for I/O ops in go.
// A SetDeadline() function is used instead.
// I like the ctx pattern better. I should understand,
// why the SetDeadline() pattern is preferred. If there's good reason for
// it, then I should switch.
func RecvMultiMsg(ctx context.Context, fd int, mmsg *C.struct_mmsg) (ret int, err error) {
LOOP:
	for ctx.Err() == nil {
		nread, readErr := C.RecvMultiMsg(C.int(fd), mmsg)
		switch {
		case nread < 0:
			errno := readErr.(syscall.Errno)
			if errno == C.EAGAIN || errno == C.EWOULDBLOCK {
				continue
			}
			ret = int(nread)
			err = fmt.Errorf("error reading message: %w", errno)
			return
		case nread == 0:
			// recvmmsg returns a message count, so 0 means nothing was
			// ready. Under MSG_DONTWAIT that is not an error; let the
			// context bound the retry.
			continue
		default:
			ret = int(nread)
			break LOOP
		}
	}

	return

}

func (s *Socket) Close() {
	// TODO: implement C equivalent for this and call it here.
}

// TODO: tests don't accept CGO:
// use of cgo in test /home/mihai/workspace/sctp-go/sctp/sockets_test.go not supported
// TODO: not sure if I should export these functions or just keep them
// for internal use.
func CreateMultiMsg(size, bufLen int) *C.struct_mmsg {
	return C.CreateMmsg(C.int(10), C.int(9216))
}

// DestroyMultiMsg frees the container and nils the caller's pointer.
//
// The double indirection is not handed to C as given. When a Go pointer is
// passed to C, the runtime scans the whole heap object it points into for
// further Go pointers — so `&someStruct.mmsg` drags in every other field, and
// panics with "cgo argument has Go pointer to unpinned Go pointer" the moment
// that struct grows a map, slice, or channel. Copying into a local first means
// C only ever sees a pointer to a lone C pointer, whatever the caller stores it
// in.
func DestroyMultiMsg(mmsg **C.struct_mmsg) {
	if mmsg == nil || *mmsg == nil {
		return
	}
	local := *mmsg
	C.DestroyMmsg(&local)
	*mmsg = local
}

// Message is one message read from an SCTP socket. Notifications are delivered
// on the same socket as user data, so the only thing telling them apart is the
// MSG_NOTIFICATION flag: without checking it, a notification looks like a short
// burst of binary garbage.
type Message struct {
	Bytes          []byte
	IsNotification bool
}

// String decodes notifications into a readable form and leaves user data alone.
func (m Message) String() string {
	if !m.IsNotification {
		return string(m.Bytes)
	}
	return NotificationString(m.Bytes)
}

// NotificationString formats a message received with MSG_NOTIFICATION set.
func NotificationString(bytes []byte) string {
	if len(bytes) == 0 {
		return ""
	}

	out := C.malloc(C.sizeof_char * 512)
	defer C.free(out)

	n := C.NotificationString(
		(*C.char)(unsafe.Pointer(&bytes[0])),
		C.int(len(bytes)),
		(*C.char)(out),
		512, /* out len */
	)
	if n < 0 {
		return fmt.Sprintf("undecodable notification, len: %d", len(bytes))
	}
	return C.GoStringN((*C.char)(out), n)
}

type MultiMsgIterator struct {
	iterator C.struct_mmsg_iterator
}

func GetMultiMsgIterator(mmsg *C.struct_mmsg) *MultiMsgIterator {
	return &MultiMsgIterator{
		iterator: C.mmsg_get_iterator(mmsg),
	}
}

func (mmit *MultiMsgIterator) Next() Message {
	mmsgBytes := C.mmsg_iterator_next(&mmit.iterator)
	return Message{
		Bytes:          C.GoBytes(unsafe.Pointer(mmsgBytes.buf), mmsgBytes.len),
		IsNotification: mmsgBytes.flags&C.MSG_NOTIFICATION != 0,
	}
}
