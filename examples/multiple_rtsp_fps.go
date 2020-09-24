package main

import (
	"github.com/peace0phmind/gfg"
	"time"
)

func main() {
	g0 := gfg.NewGfg("-rtsp_transport tcp -i rtsp://admin:Zyx123456@192.168.1.10 -vf fps=fps=1/5 test001_%06d.jpeg")
	go g0.Run()

	g1 := gfg.NewGfg("-rtsp_transport tcp -i rtsp://admin:Zyx123456@192.168.1.11 -vf fps=fps=1/10 test002_%06d.jpeg")
	go g1.Run()

	time.Sleep(4 * time.Hour)

	g0.Stop()
	g1.Stop()
}
