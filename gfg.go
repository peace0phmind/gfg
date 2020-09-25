package gfg

/*
#include "ffmpeg.h"

extern void callback_write_packet(GFFmpegContext *gc, AVPacket *pkt);
void set_cb_write_packet(GFFmpegContext *gc) {
	gc->cb_write_packet = callback_write_packet;
}
*/
import "C"
import (
	"errors"
	"fmt"
	"unsafe"
)

type Gffmpeg struct {
	cmd         string
	cb          interface{}
	gc          *C.struct_GFFmpegContext
	running     bool
	writePacket bool
	autoUseGpu  bool
}

func NewGfg(cmd string) *Gffmpeg {
	return &Gffmpeg{cmd: cmd, writePacket: true, running: false, autoUseGpu: false}
}

func NewGfgWithCb(cmd string, writePacket bool, cb interface{}) *Gffmpeg {
	return &Gffmpeg{cmd: cmd, writePacket: true, cb: cb, running: false}
}

func (g *Gffmpeg) setCallback() {
	if g.cb != nil {
		g.gc.user_data = unsafe.Pointer(g)

		if _, ok := g.cb.(CallbackWritePacket); ok {
			C.set_cb_write_packet(g.gc)
		}
	}
}

func (g *Gffmpeg) Run() error {
	if g.running {
		return errors.New("gfg is already running.")
	}

	g.running = true
	c_cmd := C.CString("gfg " + g.cmd)
	defer C.free(unsafe.Pointer(c_cmd))

	g.initGc()
	defer g.cleanGc()

	ret := C.execute_g_ffmpeg(g.gc, c_cmd)

	if ret == 0 {
		return nil
	} else {
		return g.getError()
	}
}

func (g *Gffmpeg) Stop() error {
	if !g.running {
		return errors.New("gfg must run before stop")
	}

	if g.gc != nil {
		g.gc.execute_terminated = 1
		return nil
	}

	return errors.New("gc must be init before stop")
}

func (g *Gffmpeg) IsRunning() bool {
	return g.running
}

func (g *Gffmpeg) SetWritePacket(writePacket bool) {
	g.writePacket = writePacket
}

func (g *Gffmpeg) SetCallback(cb interface{}) {
	g.cb = cb
}

func (g *Gffmpeg) SetAutoUseGpu(autoUseGpu bool) {
	g.autoUseGpu = autoUseGpu
}

func (g *Gffmpeg) IsGpuAutoUsed() bool {
	if g.running && g.gc != nil {
		return g.gc.gpu_auto_used > 0
	}

	return false
}

func (g *Gffmpeg) initGc() {
	g.gc = C.g_ffmpeg_context_init()

	if g.writePacket {
		g.gc.write_packet = 1
	} else {
		g.gc.write_packet = 0
	}

	if g.autoUseGpu {
		g.gc.auto_use_gpu = 1
	} else {
		g.gc.auto_use_gpu = 0
	}

	g.setCallback()
}

func (g *Gffmpeg) cleanGc() {
	C.av_free(unsafe.Pointer(g.gc))
	g.running = false
}

func (g *Gffmpeg) getError() error {
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
