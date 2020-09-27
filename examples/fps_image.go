package main

import (
	"github.com/peace0phmind/gfg"
	"log"
)

type CallBackTest struct {
}

func (c *CallBackTest) WritePacket(pkt *gfg.Packet) {
	log.Printf("get packet size: %d\n", pkt.Size())
}

func main() {
	g := gfg.NewGfg("-loglevel error -i ./bbb.mp4 -vf fps=fps=1/5 test%03d.jpeg")
	g.SetWritePacket(false)
	g.SetCallback(&CallBackTest{})

	g.Run()
}
