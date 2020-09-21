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
	"unsafe"
)

type gffmpeg struct {
	cmd string
	cb interface{}
    gc *C.struct_GFFmpegContext
	running bool
	writePacket bool
}

func NewGfg(cmd string, writePacket bool, cb interface{}) *gffmpeg {
	return &gffmpeg{cmd: cmd, writePacket:writePacket, cb: cb, running: false}
}

func (g *gffmpeg) setCallback() {
	if g.cb != nil {
		g.gc.user_data = unsafe.Pointer(g)

		if _, ok := g.cb.(CallbackWritePacket); ok {
			C.set_cb_write_packet(g.gc)
		}
	}
}

func (g *gffmpeg) Run() error {
	if g.running {
		return errors.New("gfg is already running.")
	}

	g.running = true
	c_cmd := C.CString("gfg " + g.cmd)
	defer C.free(unsafe.Pointer(c_cmd))

	g.gc = C.g_ffmpeg_context_init()

	if !g.writePacket {
		g.gc.write_packet = 0
	}

	g.setCallback()

	C.execute_g_ffmpeg(g.gc, c_cmd)
	g.running = false

	return nil
}

func (g *gffmpeg) Exit() error {
	if !g.running {
		return errors.New("gfg must run before exit")
	}

	if g.gc != nil {
		g.gc.execute_terminated = 1
		return nil
	}

	return errors.New("gc must be init before exit")
}

func (g *gffmpeg) IsRunning() bool {
	return g.running
}