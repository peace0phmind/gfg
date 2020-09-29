package main

import (
	"github.com/imroc/req"
	"github.com/peace0phmind/gfg"
	"log"
)

type Result struct {
	CostMs    int `json:"cost_ms"`
	ErrorCode int `json:"error_code"`
	Results   []struct {
		Confidence float64 `json:"confidence"`
		Index      int     `json:"index"`
		Label      string  `json:"label"`
		Location   struct {
			Height int `json:"height"`
			Left   int `json:"left"`
			Top    int `json:"top"`
			Width  int `json:"width"`
		} `json:"location"`
		Name  string  `json:"name"`
		Score float64 `json:"score"`
		X1    float64 `json:"x1"`
		X2    float64 `json:"x2"`
		Y1    float64 `json:"y1"`
		Y2    float64 `json:"y2"`
	} `json:"results"`
}

type BaiduSdkTest struct {
}

func (c *BaiduSdkTest) WritePacket(pkt *gfg.Packet) {
	req_param := make(req.Param)
	req_param["threshold"] = "0.1"

	r , err := req.Post("http://127.0.0.1:24401", req_param, pkt.Data())

	if err != nil {
		log.Println(err)
	}

	ret := &Result{}
	r.ToJSON(ret)
	log.Println(ret)
}

func main() {

	g := gfg.NewGfg("-rtsp_transport tcp -i rtsp://admin:Zyx123456@192.168.1.22 -vf fps=fps=1/5 test001_%06d.jpeg")

	g.SetAutoUseGpu(true)
	g.SetWritePacket(false)
	g.SetCallback(&BaiduSdkTest{})

	g.Run()

	g.Stop()
}
