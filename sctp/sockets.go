package sctp

// #cgo CFLAGS: -g -Wall -I../core/src
// #cgo LDFLAGS: -L${SRCDIR}/core/lib -lsctpcore
// #include <stdlib.h>
// #include "adapter.h"
import "C"
import (
	"context"
	"fmt"
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
	FD int
}

func NewServer(host string, port int) *Socket {
	socket := Socket{}
	fd := C.DefaultSctpServer(C.CString(host), C.uint16_t(port))
	socket.FD = int(fd)
	return &socket
}

func NewClient(host string, port int) *Socket {
	socket := Socket{}
	fd := C.DefaultSctpClient(C.CString(host), C.uint16_t(port))
	socket.FD = int(fd)
	return &socket
}

func (s *Socket) SendMsg(ctx context.Context, dst string, port int, bytes []byte) (int, error) {
	// Go []byte slice to C array
	// The C array is allocated in the C heap using malloc.
	// It is the caller's responsibility to arrange for it to be
	// freed, such as by calling C.free (be sure to include stdlib.h
	// if C.free is needed).
	cBufBytes := C.CBytes(bytes)
	defer C.free(cBufBytes)
	for ctx.Err() == nil {
		nsent, err := C.SendMsg(
			C.int(s.FD),
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

func (s *Socket) SendMultiMsg(dst string, port int, mmsg *C.struct_mmsg_t) int {
	//TODO
	return 0
}

// TODO: passing a context is not the idiomatic way for I/O ops in go.
// A SetDeadline() function is used instead.
// I like the ctx pattern better. I should understand,
// why the SetDeadline() pattern is preferred. If there's good reason for
// it, then I should switch.
// TODO: should this be blocking instead?
func (s *Socket) RecvMsg(ctx context.Context) (bytes []byte, src net.IP, port int, err error) {
	cBufBytes := C.malloc(C.sizeof_char * 9216)
	defer C.free(cBufBytes)

	cSrcBytes := C.malloc(C.sizeof_char * 100)
	defer C.free(cSrcBytes)
	saddr := C.struct_sockaddr{}
	saddr_len := C.uint(0)
LOOP:
	for ctx.Err() == nil {
		nread, readErr := C.RecvMsg(
			C.int(s.FD),
			(*C.char)(cBufBytes),
			9216, /* buf len */
			&saddr, &saddr_len,
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
			// TODO: when can this happen?
			err = fmt.Errorf("nread is 0")
		default:
			bytes = C.GoBytes(cBufBytes, nread)
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
				srcBytes := C.GoBytes(cSrcBytes, 100)
				src = net.ParseIP(string(srcBytes))
			}
			break LOOP
		}
	}

	return bytes, src, port, err
}

// TODO: passing a context is not the idiomatic way for I/O ops in go.
// A SetDeadline() function is used instead.
// I like the ctx pattern better. I should understand,
// why the SetDeadline() pattern is preferred. If there's good reason for
// it, then I should switch.
func (s *Socket) RecvMultiMsg(ctx context.Context, mmsg *C.struct_mmsg) (ret int, err error) {
LOOP:
	for ctx.Err() == nil {
		nread, readErr := C.RecvMultiMsg(C.int(s.FD), mmsg)
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
			// TODO: when can this happen?
			err = fmt.Errorf("nread is 0")
			ret = int(nread)
		default:
			ret = int(nread)
			break LOOP
		}
	}

	return

}

// TODO: tests don't accept CGO:
// use of cgo in test /home/mihai/workspace/sctp-go/sctp/sockets_test.go not supported
// TODO: not sure if I should export these functions or just keep them
// for internal use.
func CreateMultiMsg(size, bufLen int) *C.struct_mmsg {
	return C.CreateMmsg(C.int(10), C.int(9216))
}

func DestroyMultiMsg(mmsg **C.struct_mmsg) {
	C.DestroyMmsg(mmsg)
}

type MultiMsgIterator struct {
	iterator C.struct_mmsg_iterator
}

func GetMultiMsgIterator(mmsg *C.struct_mmsg) *MultiMsgIterator {
	return &MultiMsgIterator{
		iterator: C.mmsg_get_iterator(mmsg),
	}
}

func (mmit *MultiMsgIterator) Next() []byte {
	mmsgBytes := C.mmsg_iterator_next(&mmit.iterator)
	return C.GoBytes(unsafe.Pointer(mmsgBytes.buf), mmsgBytes.len)
}
