package gfg

/*
#include "ffmpeg.h"
 */
import "C"
import "unsafe"

type gfg struct {
    gc *C.struct_GFFmpegContext
}

func NewGfg() *gfg {
	ret := &gfg{}
	ret.gc = C.g_ffmpeg_context_init()

	return ret
}

func (g *gfg) Run(cmd string) error {
	c_cmd := C.CString("gfg " + cmd)
	defer C.free(unsafe.Pointer(c_cmd))

	C.execute_g_ffmpeg(g.gc, c_cmd)
	return nil
}