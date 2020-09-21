package main


import "C"
import (
	"github.com/peace0phmind/gfg"
)

type CallBackTest struct {
}

func (c *CallBackTest) WritePacket(pkt *C.struct_AVPacket) {
	//log.Printf("get packet size: %d", pkt.size)
}

func main() {
	g := gfg.NewGfg("-loglevel debug -i /home/test/go/src/github.com/peace0phmind/gmf/examples/bbb.mp4 -vf fps=fps=1/5 test%03d.jpeg", false, &CallBackTest{})

	g.Run()
}