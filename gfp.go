package gfg

/*
#include "ffmpeg.h"
*/
import "C"
import (
	"errors"
	"fmt"
	"unsafe"
)

type Gffprobe struct {
	cmd     string
	gc      *C.struct_GFFmpegContext
	running bool
}

type FormatInfo struct {
}

func NewGfp(cmd string) *Gffprobe {
	return &Gffprobe{cmd: cmd}
}

func (g *Gffprobe) getInfo() *FormatInfo {
	return &FormatInfo{}
}

func (g *Gffprobe) GetInfo() (*FormatInfo, error) {
	if g.running {
		return nil, errors.New("gfg is already running.")
	}

	g.running = true
	c_cmd := C.CString("gfp " + g.cmd)
	defer C.free(unsafe.Pointer(c_cmd))

	g.initGc()
	defer g.cleanGc()

	ret := C.execute_g_ffprobe(g.gc, c_cmd)

	if ret == 0 {
		return g.getInfo(), nil
	} else {
		return nil, g.getError()
	}
}

func (g *Gffprobe) initGc() {
	g.gc = C.g_ffmpeg_context_init()
}

func (g *Gffprobe) cleanGc() {
	C.av_free(unsafe.Pointer(g.gc))
	g.running = false
}

func (g *Gffprobe) getError() error {
	if g.gc.last_error == 0 {
		return nil
	} else if g.gc.last_error < 0 {
		if g.gc.last_error_ptr == nil {
			return errors.New(C.GoString(&g.gc.last_error_buf[0]))
		} else {
			return errors.New(C.GoString(g.gc.last_error_ptr))
		}
	} else {
		return errors.New(fmt.Sprintf("gfg get error no: %d", g.gc.last_error))
	}
}
