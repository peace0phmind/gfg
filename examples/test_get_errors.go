package main

import (
	"github.com/peace0phmind/gfg"
	"log"
)

func main() {
	g := gfg.NewGfg("-rtsp_transport tcp -i rtsp://admin:Zyx123456@192.168.1.101 -vf fps=fps=1/5 test001_%06d.jpeg")
	if err := g.Run(); err != nil {
		log.Println(err)
	}
}