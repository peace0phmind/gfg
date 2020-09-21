package gfg

/*
#include "ffmpeg.h"
 */
import "C"

type CallbackWritePacket interface {
	WritePacket(pkt *C.struct_AVPacket)
}

//export callback_write_packet
func callback_write_packet(gc *C.struct_GFFmpegContext, pkt *C.struct_AVPacket) {
	g := (*gffmpeg)(gc.user_data)
	if callback, ok := g.cb.(CallbackWritePacket); ok {
		callback.WritePacket(pkt)
	}
}