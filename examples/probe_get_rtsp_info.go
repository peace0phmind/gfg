package main

import (
	"github.com/peace0phmind/gfg"
	"log"
)

func main() {
	p := gfg.NewGfp("-show_log 1 rtsp://admin:Zyx123456@192.168.1.10")
	if info, err := p.GetInfo(); err != nil {
		log.Fatalln(err)
	} else {
		log.Println(info)
	}
}