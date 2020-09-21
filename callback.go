package gfg

/*
#include "ffmpeg.h"
 */
import "C"
import "unsafe"

type Packet struct {
	avPacket *C.struct_AVPacket
}

func (p *Packet) Pts() int64 {
	return int64(p.avPacket.pts)
}

func (p *Packet) Dts() int64 {
	return int64(p.avPacket.dts)
}

func (p *Packet) Flags() int {
	return int(p.avPacket.flags)
}

func (p *Packet) Duration() int64 {
	return int64(p.avPacket.duration)
}

func (p *Packet) Size() int {
	return int(p.avPacket.size)
}

func (p *Packet) Pos() int64 {
	return int64(p.avPacket.pos)
}

func (p *Packet) Data() []byte {
	return C.GoBytes(unsafe.Pointer(p.avPacket.data), C.int(p.avPacket.size))
}

type CallbackWritePacket interface {
	WritePacket(pkt *Packet)
}

//export callback_write_packet
func callback_write_packet(gc *C.struct_GFFmpegContext, pkt *C.struct_AVPacket) {
	g := (*gffmpeg)(gc.user_data)
	if callback, ok := g.cb.(CallbackWritePacket); ok {
		p := &Packet{avPacket: pkt}
		callback.WritePacket(p)
		p.avPacket = nil
	}
}