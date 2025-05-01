package main

/*
#include <stdlib.h>
// opaque runner handle
typedef void* RunnerHandle;
*/
import "C"
import (
	"bytes"
	"context"
	"io/ioutil"
	"unsafe"

	"mvdan.cc/sh/v3/interp"
	"mvdan.cc/sh/v3/syntax"
)

//export RunScript
func RunScript(path *C.char) C.int {
	fname := C.GoString(path)
	src, err := ioutil.ReadFile(fname)
	if err != nil {
		return 1
	}
	file, err := syntax.NewParser().Parse(bytes.NewReader(src), fname)
	if err != nil {
		return 1
	}
	// obtain runner handle, then cast back to Go pointer
	rh := NewRunner()
	runner := (*interp.Runner)(unsafe.Pointer(rh))
	if err := runner.Run(context.Background(), file); err != nil {
		return 1
	}
	return 0
}

//export NewRunner
func NewRunner() C.RunnerHandle {
	r, err := interp.New()
	if err != nil {
		panic(err)
	}
	return C.RunnerHandle(unsafe.Pointer(r))
}

func main() {}
