/*
 * Various utilities for command line tools
 * copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef FFTOOLS_CMDUTILS_H
#define FFTOOLS_CMDUTILS_H

#include <stdint.h>
#include <setjmp.h>

#include "config.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#ifdef _WIN32
#undef main /* We don't want SDL to override our main() */
#endif

#define JUMP_BUFFER_SUCCESS 0x88888888

typedef struct GFFmpegContext GFFmpegContext;

/**
 * Register a program-specific cleanup routine.
 */
void register_exit(GFFmpegContext *gc, void (*cb)(GFFmpegContext *gc, int ret));

/**
 * Wraps exit with a program-specific cleanup routine.
 */
void exit_program(GFFmpegContext *gc, int ret) av_noreturn;

/**
 * Initialize dynamic library loading
 */
void init_dynload(void);

/**
 * Initialize the cmdutils option system, in particular
 * allocate the *_opts contexts.
 */
void init_opts(GFFmpegContext *gc);
/**
 * Uninitialize the cmdutils option system, in particular
 * free the *_opts contexts and their contents.
 */
void uninit_opts(GFFmpegContext *gc);

/**
 * Trivial log callback.
 * Only suitable for opt_help and similar since it lacks prefix handling.
 */
void log_callback_help(void* ptr, int level, const char* fmt, va_list vl);

/**
 * Override the cpuflags.
 */
int opt_cpuflags(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg);

/**
 * Fallback for options that are not explicitly handled, these will be
 * parsed through AVOptions.
 */
int opt_default(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg);

/**
 * Set the libav* libraries log level.
 */
int opt_loglevel(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg);

int opt_report(void *optctx, const char *opt, const char *arg);

int opt_max_alloc(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg);

/**
 * Limit the execution time.
 */
int opt_timelimit(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg);

/**
 * Parse a string and return its corresponding value as a double.
 * Exit from the application if the string cannot be correctly
 * parsed or the corresponding value is invalid.
 *
 * @param context the context of the value to be set (e.g. the
 * corresponding command line option name)
 * @param numstr the string to be parsed
 * @param type the type (OPT_INT64 or OPT_FLOAT) as which the
 * string should be parsed
 * @param min the minimum valid accepted value
 * @param max the maximum valid accepted value
 */
double parse_number_or_die(GFFmpegContext *gc, const char *context, const char *numstr, int type,
                           double min, double max);

/**
 * Parse a string specifying a time and return its corresponding
 * value as a number of microseconds. Exit from the application if
 * the string cannot be correctly parsed.
 *
 * @param context the context of the value to be set (e.g. the
 * corresponding command line option name)
 * @param timestr the string to be parsed
 * @param is_duration a flag which tells how to interpret timestr, if
 * not zero timestr is interpreted as a duration, otherwise as a
 * date
 *
 * @see av_parse_time()
 */
int64_t parse_time_or_die(GFFmpegContext *gc, const char *context, const char *timestr,
                          int is_duration);

typedef struct SpecifierOpt {
    char *specifier;    /**< stream/chapter/program/... specifier */
    union {
        uint8_t *str;
        int        i;
        int64_t  i64;
        uint64_t ui64;
        float      f;
        double   dbl;
    } u;
} SpecifierOpt;

typedef struct OptionDef {
    const char *name;
    int flags;
#define HAS_ARG    0x0001
#define OPT_BOOL   0x0002
#define OPT_EXPERT 0x0004
#define OPT_STRING 0x0008
#define OPT_VIDEO  0x0010
#define OPT_AUDIO  0x0020
#define OPT_INT    0x0080
#define OPT_FLOAT  0x0100
#define OPT_SUBTITLE 0x0200
#define OPT_INT64  0x0400
#define OPT_EXIT   0x0800
#define OPT_DATA   0x1000
#define OPT_PERFILE  0x2000     /* the option is per-file (currently ffmpeg-only).
                                   implied by OPT_OFFSET or OPT_SPEC */
#define OPT_OFFSET 0x4000       /* option is specified as an offset in a passed optctx */
#define OPT_SPEC   0x8000       /* option is to be stored in an array of SpecifierOpt.
                                   Implies OPT_OFFSET. Next element after the offset is
                                   an int containing element count in the array. */
#define OPT_TIME  0x10000
#define OPT_DOUBLE 0x20000
#define OPT_INPUT  0x40000
#define OPT_OUTPUT 0x80000

#define OPT_G_OFFSET 0x800000
    union {
        void *dst_ptr;
        int (*func_arg)(GFFmpegContext *gc, void *, const char *, const char *);
        size_t off;
    } u;
    const char *help;
    const char *argname;
} OptionDef;

#define CMDUTILS_COMMON_OPTIONS                                                                                         \
    { "loglevel",    HAS_ARG,              { .func_arg = opt_loglevel },     "set logging level", "loglevel" },         \
    { "v",           HAS_ARG,              { .func_arg = opt_loglevel },     "set logging level", "loglevel" },         \
    { "max_alloc",   HAS_ARG,              { .func_arg = opt_max_alloc },    "set maximum size of a single allocated block", "bytes" }, \
    { "cpuflags",    HAS_ARG | OPT_EXPERT, { .func_arg = opt_cpuflags },     "force specific cpu flags", "flags" },     \


/**
 * Parse the command line arguments.
 *
 * @param optctx an opaque options context
 * @param argc   number of command line arguments
 * @param argv   values of command line arguments
 * @param options Array with the definitions required to interpret every
 * option of the form: -option_name [argument]
 * @param parse_arg_function Name of the function called to process every
 * argument without a leading option name flag. NULL if such arguments do
 * not have to be processed.
 */
void parse_options(GFFmpegContext *gc, void *optctx, int argc, char **argv, const OptionDef *options,
                   void (* parse_arg_function)(GFFmpegContext *, void *optctx, const char*));

/**
 * Parse one given option.
 *
 * @return on success 1 if arg was consumed, 0 otherwise; negative number on error
 */
int parse_option(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg,
                 const OptionDef *options);

/**
 * An option extracted from the commandline.
 * Cannot use AVDictionary because of options like -map which can be
 * used multiple times.
 */
typedef struct Option {
    const OptionDef  *opt;
    const char       *key;
    const char       *val;
} Option;

typedef struct OptionGroupDef {
    /**< group name */
    const char *name;
    /**
     * Option to be used as group separator. Can be NULL for groups which
     * are terminated by a non-option argument (e.g. ffmpeg output files)
     */
    const char *sep;
    /**
     * Option flags that must be set on each option that is
     * applied to this group
     */
    int flags;
} OptionGroupDef;

typedef struct OptionGroup {
    const OptionGroupDef *group_def;
    const char *arg;

    Option *opts;
    int  nb_opts;

    AVDictionary *codec_opts;
    AVDictionary *format_opts;
    AVDictionary *resample_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
} OptionGroup;

/**
 * A list of option groups that all have the same group type
 * (e.g. input files or output files)
 */
typedef struct OptionGroupList {
    const OptionGroupDef *group_def;

    OptionGroup *groups;
    int       nb_groups;
} OptionGroupList;

typedef struct OptionParseContext {
    OptionGroup global_opts;

    OptionGroupList *groups;
    int           nb_groups;

    /* parsing state */
    OptionGroup cur_group;
} OptionParseContext;

/**
 * Parse an options group and write results into optctx.
 *
 * @param optctx an app-specific options context. NULL for global options group
 */
int parse_optgroup(GFFmpegContext *gc, void *optctx, OptionGroup *g);

/**
 * Split the commandline into an intermediate form convenient for further
 * processing.
 *
 * The commandline is assumed to be composed of options which either belong to a
 * group (those with OPT_SPEC, OPT_OFFSET or OPT_PERFILE) or are global
 * (everything else).
 *
 * A group (defined by an OptionGroupDef struct) is a sequence of options
 * terminated by either a group separator option (e.g. -i) or a parameter that
 * is not an option (doesn't start with -). A group without a separator option
 * must always be first in the supplied groups list.
 *
 * All options within the same group are stored in one OptionGroup struct in an
 * OptionGroupList, all groups with the same group definition are stored in one
 * OptionGroupList in OptionParseContext.groups. The order of group lists is the
 * same as the order of group definitions.
 */
int split_commandline(GFFmpegContext *gc, OptionParseContext *octx, int argc, char *argv[],
                      const OptionDef *options,
                      const OptionGroupDef *groups, int nb_groups);

/**
 * Free all allocated memory in an OptionParseContext.
 */
void uninit_parse_context(GFFmpegContext *gc, OptionParseContext *octx);

/**
 * Find the '-loglevel' option in the command line args and apply it.
 */
void parse_loglevel(GFFmpegContext *gc, int argc, char **argv, const OptionDef *options);

/**
 * Return index of option opt in argv or 0 if not found.
 */
int locate_option(int argc, char **argv, const OptionDef *options,
                  const char *optname);

/**
 * Check if the given stream matches a stream specifier.
 *
 * @param s  Corresponding format context.
 * @param st Stream from s to be checked.
 * @param spec A stream specifier of the [v|a|s|d]:[\<stream index\>] form.
 *
 * @return 1 if the stream matches, 0 if it doesn't, <0 on error
 */
int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);

/**
 * Filter out options for given codec.
 *
 * Create a new options dictionary containing only the options from
 * opts which apply to the codec with ID codec_id.
 *
 * @param opts     dictionary to place options in
 * @param codec_id ID of the codec that should be filtered for
 * @param s Corresponding format context.
 * @param st A stream from s for which the options should be filtered.
 * @param codec The particular codec for which the options should be filtered.
 *              If null, the default one is looked up according to the codec id.
 * @return a pointer to the created dictionary
 */
AVDictionary *filter_codec_opts(GFFmpegContext *gc, AVDictionary *opts, enum AVCodecID codec_id,
                                AVFormatContext *s, AVStream *st, AVCodec *codec);

/**
 * Setup AVCodecContext options for avformat_find_stream_info().
 *
 * Create an array of dictionaries, one dictionary for each stream
 * contained in s.
 * Each dictionary will contain the options from codec_opts which can
 * be applied to the corresponding stream codec context.
 *
 * @return pointer to the created array of dictionaries, NULL if it
 * cannot be created
 */
AVDictionary **setup_find_stream_info_opts(GFFmpegContext *gc, AVFormatContext *s,
                                           AVDictionary *codec_opts);

/**
 * Print an error message to stderr, indicating filename and a human
 * readable description of the error code err.
 *
 * If strerror_r() is not available the use of this function in a
 * multithreaded application may be unsafe.
 *
 * @see av_strerror()
 */
void print_error(const char *filename, int err);


/**
 * Return a positive value if a line read from standard input
 * starts with [yY], otherwise return 0.
 */
int read_yesno(void);

/**
 * Get a file corresponding to a preset file.
 *
 * If is_path is non-zero, look for the file in the path preset_name.
 * Otherwise search for a file named arg.ffpreset in the directories
 * $FFMPEG_DATADIR (if set), $HOME/.ffmpeg, and in the datadir defined
 * at configuration time or in a "ffpresets" folder along the executable
 * on win32, in that order. If no such file is found and
 * codec_name is defined, then search for a file named
 * codec_name-preset_name.avpreset in the above-mentioned directories.
 *
 * @param filename buffer where the name of the found filename is written
 * @param filename_size size in bytes of the filename buffer
 * @param preset_name name of the preset to search
 * @param is_path tell if preset_name is a filename path
 * @param codec_name name of the codec for which to look for the
 * preset, may be NULL
 */
FILE *get_preset_file(char *filename, size_t filename_size,
                      const char *preset_name, int is_path, const char *codec_name);

/**
 * Realloc array to hold new_size elements of elem_size.
 * Calls exit() on failure.
 *
 * @param array array to reallocate
 * @param elem_size size in bytes of each element
 * @param size new element count will be written here
 * @param new_size number of elements to place in reallocated array
 * @return reallocated array
 */
void *grow_array(GFFmpegContext *gc, void *array, int elem_size, int *size, int new_size);

#define media_type_string av_get_media_type_string

#define GROW_ARRAY(gc, array, nb_elems)\
    array = grow_array(gc, array, sizeof(*array), &nb_elems, nb_elems + 1)

#define GET_PIX_FMT_NAME(pix_fmt)\
    const char *name = av_get_pix_fmt_name(pix_fmt);

#define GET_CODEC_NAME(id)\
    const char *name = avcodec_descriptor_get(id)->name;

#define GET_SAMPLE_FMT_NAME(sample_fmt)\
    const char *name = av_get_sample_fmt_name(sample_fmt)

#define GET_SAMPLE_RATE_NAME(rate)\
    char name[16];\
    snprintf(name, sizeof(name), "%d", rate);

#define GET_CH_LAYOUT_NAME(ch_layout)\
    char name[16];\
    snprintf(name, sizeof(name), "0x%"PRIx64, ch_layout);

#define GET_CH_LAYOUT_DESC(ch_layout)\
    char name[128];\
    av_get_channel_layout_string(name, sizeof(name), 0, ch_layout);

double get_rotation(AVStream *st);

#define G_OFFSET(x) offsetof(GFFmpegContext, x)

char **parsedargs(char *args, int *argc);
void freeparsedargs(char **argv);
int enter_program(GFFmpegContext *gc, int argc, char **argv, int (*main_func)(GFFmpegContext *gc, int argc, char **argv));

AVCodec *try_auto_use_gpu_by_name(GFFmpegContext *gc, char *name, int is_encoder);
AVCodec *try_auto_use_gpu_by_codec_id(GFFmpegContext *gc, enum AVCodecID codec_id, int is_encoder);

#endif /* FFTOOLS_CMDUTILS_H */
