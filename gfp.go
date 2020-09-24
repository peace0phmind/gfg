package gfg

/*
#include "ffmpeg.h"
*/
import "C"
import (
	"errors"
	"fmt"
	"unsafe"
)

type gffprobe struct {
	cmd     string
	gc      *C.struct_GFFmpegContext
	running bool
}

//typedef struct {
// * number of reference frames
// * - decoding: Set by lavc.
// */
//int refs;
//
///* video only */
//enum AVPixelFormat pix_fmt;
//int width, height;
//int coded_width, coded_height;
//int fps;
//
///* audio only */
//enum AVSampleFormat sample_fmt;  ///< sample format
//int sample_rate; ///< samples per second
//int channels;    ///< number of audio channels
//int64_t bitrate;
//
//} Probe_Stream_Info;
//

type StreamInfo struct {
	MediaType int
	CodecId int
	Refs int
	// under is video
	PixFmt int
	Width, Height int
	CodedWidth, CodedHeight int
	Fps int
	// under is audio
	SampleFmt int
	SampleRate int
	Channels int
	Bitrate int64
}

type FormatInfo struct {
	FormatName string
	FileName string
	StreamInfos []*StreamInfo
}

func GetInfo(cmd string) (*FormatInfo, error) {
	g := &gffprobe{cmd: cmd, running: false}
	return g.getInfo()
}

func (g *gffprobe) getProbeInfo() *FormatInfo {
	fi := &FormatInfo{}

	fi.FormatName = C.GoString(&g.gc.format_info.format_name[0])
	fi.FileName = C.GoString(&g.gc.format_info.file_name[0])

	for i := 0; i < int(g.gc.format_info.nb_stream_info); i++ {
		si := g.gc.format_info.stream_infos[i]
		gsi := &StreamInfo{}
		gsi.MediaType = int(si.media_type)
		gsi.CodecId = int(si.codec_id)
		gsi.Refs = int(si.refs)
		gsi.PixFmt = int(si.pix_fmt)
		gsi.Width = int(si.width)
		gsi.Height = int(si.height)
		gsi.CodedWidth = int(si.coded_width)
		gsi.CodedHeight = int(si.coded_height)
		gsi.Fps = int(si.fps)
		gsi.SampleFmt = int(si.sample_fmt)
		gsi.SampleRate = int(si.sample_rate)
		gsi.Channels = int(si.channels)
		gsi.Bitrate = int64(si.bitrate)

		fi.StreamInfos = append(fi.StreamInfos, gsi)
	}

	return fi
}

func (g *gffprobe) getInfo() (*FormatInfo, error) {
	if g.running {
		return nil, errors.New("gfg is already running.")
	}

	g.running = true
	c_cmd := C.CString("gfp " + g.cmd)
	defer C.free(unsafe.Pointer(c_cmd))

	g.initGc()
	defer g.cleanGc()

	ret := C.execute_g_ffprobe(g.gc, c_cmd)

	if ret == 0 {
		return g.getProbeInfo(), nil
	} else {
		return nil, g.getError()
	}
}

func (g *gffprobe) initGc() {
	g.gc = C.g_ffmpeg_context_init()
}

func (g *gffprobe) cleanGc() {
	C.av_free(unsafe.Pointer(g.gc))
	g.running = false
}

func (g *gffprobe) getError() error {
	if g.gc.last_error == 0 {
		return nil
	} else if g.gc.last_error < 0 {
		if g.gc.last_error_ptr == nil {
			return errors.New(C.GoString(&g.gc.last_error_buf[0]))
		} else {
			return errors.New(C.GoString(g.gc.last_error_ptr))
		}
	} else {
		return errors.New(fmt.Sprintf("gfg get error no: %d", g.gc.last_error))
	}
}
