package main

import (
	"github.com/peace0phmind/gfg"
	"log"
	"time"
)

type AutoGpuCallBackTest struct {
}

func (c *AutoGpuCallBackTest) WritePacket(pkt *gfg.Packet) {
	log.Printf("get packet size: %d\n", pkt.Size())
}

func main() {
	g := gfg.NewGfg("-rtsp_transport tcp -i rtsp://admin:Zyx123456@192.168.1.10 -vf fps=fps=1/5 test001_%06d.jpeg")

	g.SetAutoUseGpu(true)
	g.SetWritePacket(false)
	g.SetCallback(&AutoGpuCallBackTest{})

	go g.Run()

	time.Sleep(5 * time.Second)
	log.Printf("Gpu Auto used: %V, decoder name: %s", g.IsGpuAutoUsed(), g.GetGpuDecoderName())

	time.Sleep(5 * time.Second)
	log.Printf("Gpu Auto used: %V, decoder name: %s", g.IsGpuAutoUsed(), g.GetGpuDecoderName())

	time.Sleep(5 * time.Second)
	log.Printf("Gpu Auto used: %V, decoder name: %s", g.IsGpuAutoUsed(), g.GetGpuDecoderName())

	time.Sleep(5 * time.Hour)

	g.Stop()
}
