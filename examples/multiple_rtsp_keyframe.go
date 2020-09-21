package main

import (
	"fmt"
	"github.com/peace0phmind/gfg"
	"log"
	"os"
	"time"
)

type RtspBackTest struct {
	Code string
	Gc *gfg.Gffmpeg
	fileCount int
	Running bool
}

func (c *RtspBackTest)writeFile(b []byte) {
	name := fmt.Sprintf("./test%s_%06d.jpeg", c.Code,c.fileCount)

	fp, err := os.OpenFile(name, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0644)
	if err != nil {
		log.Fatalf("%s\n", err)
	}

	if n, err := fp.Write(b); err != nil {
		log.Fatalf("%s\n", err)
	} else {
		log.Printf("%d bytes written to '%s'", n, name)
	}

	fp.Close()

	c.fileCount++
}

func (c *RtspBackTest) WritePacket(pkt *gfg.Packet) {
	log.Printf("get packet size: %d\n", pkt.Size())
	c.writeFile(pkt.Data())
	c.Gc.Stop()
}

func main() {
	r0 := &RtspBackTest{Code: "001", Running : true}
	g0 := gfg.NewGfg("-i rtsp://admin:Zyx123456@192.168.1.10 -vf select='eq(pict_type\\,I)' test%03d.jpeg", false, r0)
	r0.Gc = g0

	go func() {
		for ;r0.Running; {
			g0.Run()
			time.Sleep(4*time.Second)
		}
	}()

	r1 := &RtspBackTest{Code: "002", Running: true}
	g1 := gfg.NewGfg("-i rtsp://admin:Zyx123456@192.168.1.11 -vf select='eq(pict_type\\,I)' test%03d.jpeg", false, r1)
	r1.Gc = g1

	go func() {
		for ;r1.Running; {
			g1.Run()
			time.Sleep(9*time.Second)
		}
	}()

	time.Sleep(4*time.Hour)

	r0.Running = false
	r1.Running = false

	g0.Stop()
	g1.Stop()
}