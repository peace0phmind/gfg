package main

import (
	"github.com/peace0phmind/gfg"
	"log"
)

func main() {
	if info, err := gfg.GetInfo("-show_log 1 rtsp://admin:Zyx123456@192.168.1.10"); err != nil {
		log.Fatalln(err)
	} else {
		log.Println(info)
	}
}