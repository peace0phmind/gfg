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
	g := gfg.NewGfg("-loglevel error -i /home/test/go/src/github.com/peace0phmind/gmf/examples/bbb.mp4 -vf fps=fps=1/5 test%03d.jpeg", false, &CallBackTest{})

	g.Run()
}