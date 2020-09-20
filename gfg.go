package gfg

/*
#include "ffmpeg.h"
 */
import "C"
import "unsafe"

type gfg struct {

}

func NewGfg() *gfg {
	return &gfg{}
}

func (g *gfg) Run(cmd string) error {
	c_cmd := C.CString("gfg " + cmd)
	defer C.free(unsafe.Pointer(c_cmd))

	C.execute_g_ffmpeg(c_cmd)
	return nil
}