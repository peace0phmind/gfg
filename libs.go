// +build !shared

package gfg

/*
#include "config.h"

#ifdef ARCH_ARM
#cgo LDFLAGS: -L/opt/vc/lib/
#endif

#cgo pkg-config: libavcodec libavformat libavutil libavdevice libswscale libswresample
*/
import "C"
