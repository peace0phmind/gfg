package main

import "github.com/peace0phmind/gfg"

func main() {
	g := gfg.NewGfg();

	g.Run("-loglevel debug -i /home/test/go/src/github.com/peace0phmind/gmf/examples/bbb.mp4 -vf fps=fps=1/5 test%03d.jpeg")
}