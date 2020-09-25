/*
 * Various utilities for command line tools
 * Copyright (c) 2000-2003 Fabrice Bellard
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>

/* Include only the enabled headers since some compilers (namely, Sun
   Studio) will not omit unused inline functions and create undefined
   references to libraries that are not being built. */

#include "config.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/display.h"
#include "libavutil/parseutils.h"
#include "libavutil/eval.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "libavutil/cpu.h"
#include "libavutil/version.h"
#include "cmdutils.h"
#include "ffmpeg.h"
#if HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

static int init_report(const char *env);

enum show_muxdemuxers {
    SHOW_DEFAULT,
    SHOW_DEMUXERS,
    SHOW_MUXERS,
};

void init_opts(GFFmpegContext *gc)
{
    av_dict_set(&gc->sws_dict, "flags", "bicubic", 0);
}

void uninit_opts(GFFmpegContext *gc)
{
    av_dict_free(&gc->swr_opts);
    av_dict_free(&gc->sws_dict);
    av_dict_free(&gc->format_opts);
    av_dict_free(&gc->codec_opts);
    av_dict_free(&gc->resample_opts);
}

void log_callback_help(void *ptr, int level, const char *fmt, va_list vl)
{
    vfprintf(stdout, fmt, vl);
}

void init_dynload(void)
{
#if HAVE_SETDLLDIRECTORY && defined(_WIN32)
    /* Calling SetDllDirectory with the empty string (but not NULL) removes the
     * current working directory from the DLL search path as a security pre-caution. */
    SetDllDirectory("");
#endif
}

void register_exit(GFFmpegContext *gc, void (*cb)(GFFmpegContext *gc, int ret))
{
    gc->program_exit = cb;
}

void exit_program(GFFmpegContext *gc, int ret)
{
    if (gc->program_exit)
        gc->program_exit(gc, ret);

    av_log(NULL, AV_LOG_INFO, "Program exit with ret: %d.\n", ret);

    gc->last_error = ret;

    if (ret < 0) {
        gc->last_error_ptr = NULL;
        if (av_strerror(ret, gc->last_error_buf, LAST_ERROR_BUF_SIZE) < 0) {
            gc->last_error_ptr = strerror(AVUNERROR(ret));
        }
    }

    longjmp(gc->_jmp_buf, ret == 0 ? JUMP_BUFFER_SUCCESS : ret);
}

double parse_number_or_die(GFFmpegContext *gc, const char *context, const char *numstr, int type,
                           double min, double max)
{
    char *tail;
    const char *error;
    double d = av_strtod(numstr, &tail);
    if (*tail)
        error = "Expected number for %s but found: %s\n";
    else if (d < min || d > max)
        error = "The value for %s was %s which is not within %f - %f\n";
    else if (type == OPT_INT64 && (int64_t)d != d)
        error = "Expected int64 for %s but found %s\n";
    else if (type == OPT_INT && (int)d != d)
        error = "Expected int for %s but found %s\n";
    else
        return d;
    av_log(NULL, AV_LOG_FATAL, error, context, numstr, min, max);
    exit_program(gc, 1);
    return 0;
}

int64_t parse_time_or_die(GFFmpegContext *gc, const char *context, const char *timestr,
                          int is_duration)
{
    int64_t us;
    int err;
    if ((err = av_parse_time(&us, timestr, is_duration)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Invalid %s specification for %s: %s\n",
               is_duration ? "duration" : "date", context, timestr);
        exit_program(gc, err);
    }
    return us;
}

static const OptionDef *find_option(const OptionDef *po, const char *name)
{
    const char *p = strchr(name, ':');
    int len = p ? p - name : strlen(name);

    while (po->name) {
        if (!strncmp(name, po->name, len) && strlen(po->name) == len)
            break;
        po++;
    }
    return po;
}

/* _WIN32 means using the windows libc - cygwin doesn't define that
 * by default. HAVE_COMMANDLINETOARGVW is true on cygwin, while
 * it doesn't provide the actual command line via GetCommandLineW(). */
#if HAVE_COMMANDLINETOARGVW && defined(_WIN32)
#include <shellapi.h>
/* Will be leaked on exit */
static char** win32_argv_utf8 = NULL;
static int win32_argc = 0;

/**
 * Prepare command line arguments for executable.
 * For Windows - perform wide-char to UTF-8 conversion.
 * Input arguments should be main() function arguments.
 * @param argc_ptr Arguments number (including executable)
 * @param argv_ptr Arguments list.
 */
static void prepare_app_arguments(int *argc_ptr, char ***argv_ptr)
{
    char *argstr_flat;
    wchar_t **argv_w;
    int i, buffsize = 0, offset = 0;

    if (win32_argv_utf8) {
        *argc_ptr = win32_argc;
        *argv_ptr = win32_argv_utf8;
        return;
    }

    win32_argc = 0;
    argv_w = CommandLineToArgvW(GetCommandLineW(), &win32_argc);
    if (win32_argc <= 0 || !argv_w)
        return;

    /* determine the UTF-8 buffer size (including NULL-termination symbols) */
    for (i = 0; i < win32_argc; i++)
        buffsize += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                        NULL, 0, NULL, NULL);

    win32_argv_utf8 = av_mallocz(sizeof(char *) * (win32_argc + 1) + buffsize);
    argstr_flat     = (char *)win32_argv_utf8 + sizeof(char *) * (win32_argc + 1);
    if (!win32_argv_utf8) {
        LocalFree(argv_w);
        return;
    }

    for (i = 0; i < win32_argc; i++) {
        win32_argv_utf8[i] = &argstr_flat[offset];
        offset += WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1,
                                      &argstr_flat[offset],
                                      buffsize - offset, NULL, NULL);
    }
    win32_argv_utf8[i] = NULL;
    LocalFree(argv_w);

    *argc_ptr = win32_argc;
    *argv_ptr = win32_argv_utf8;
}
#else
static inline void prepare_app_arguments(int *argc_ptr, char ***argv_ptr)
{
    /* nothing to do */
}
#endif /* HAVE_COMMANDLINETOARGVW */

static int write_option(GFFmpegContext *gc, void *optctx, const OptionDef *po, const char *opt,
                        const char *arg)
{
    /* new-style options contain an offset into optctx, old-style address of
     * a global var*/
    void *dst = NULL;
    if (po->flags & (OPT_OFFSET | OPT_SPEC)) {
        dst = (uint8_t *)optctx + po->u.off;
    } else if (po->flags & OPT_G_OFFSET) {
        dst = (uint8_t *)gc + po->u.off;
    } else {
        dst = po->u.dst_ptr;
    }

    int *dstcount;

    if (po->flags & OPT_SPEC) {
        SpecifierOpt **so = dst;
        char *p = strchr(opt, ':');
        char *str;

        dstcount = (int *)(so + 1);
        *so = grow_array(gc, *so, sizeof(**so), dstcount, *dstcount + 1);
        str = av_strdup(p ? p + 1 : "");
        if (!str)
            return AVERROR(ENOMEM);
        (*so)[*dstcount - 1].specifier = str;
        dst = &(*so)[*dstcount - 1].u;
    }

    if (po->flags & OPT_STRING) {
        char *str;
        str = av_strdup(arg);
        av_freep(dst);
        if (!str)
            return AVERROR(ENOMEM);
        *(char **)dst = str;
    } else if (po->flags & OPT_BOOL || po->flags & OPT_INT) {
        *(int *)dst = parse_number_or_die(gc, opt, arg, OPT_INT64, INT_MIN, INT_MAX);
    } else if (po->flags & OPT_INT64) {
        *(int64_t *)dst = parse_number_or_die(gc, opt, arg, OPT_INT64, INT64_MIN, INT64_MAX);
    } else if (po->flags & OPT_TIME) {
        *(int64_t *)dst = parse_time_or_die(gc, opt, arg, 1);
    } else if (po->flags & OPT_FLOAT) {
        *(float *)dst = parse_number_or_die(gc, opt, arg, OPT_FLOAT, -INFINITY, INFINITY);
    } else if (po->flags & OPT_DOUBLE) {
        *(double *)dst = parse_number_or_die(gc, opt, arg, OPT_DOUBLE, -INFINITY, INFINITY);
    } else if (po->u.func_arg) {
        int ret = po->u.func_arg(gc, optctx, opt, arg);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to set value '%s' for option '%s': %s\n",
                   arg, opt, av_err2str(ret));
            return ret;
        }
    }
    if (po->flags & OPT_EXIT)
        exit_program(gc, 0);

    return 0;
}

int parse_option(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg,
                 const OptionDef *options)
{
    const OptionDef *po;
    int ret;

    po = find_option(options, opt);
    if (!po->name && opt[0] == 'n' && opt[1] == 'o') {
        /* handle 'no' bool option */
        po = find_option(options, opt + 2);
        if ((po->name && (po->flags & OPT_BOOL)))
            arg = "0";
    } else if (po->flags & OPT_BOOL)
        arg = "1";

    if (!po->name)
        po = find_option(options, "default");
    if (!po->name) {
        av_log(NULL, AV_LOG_ERROR, "Unrecognized option '%s'\n", opt);
        return AVERROR(EINVAL);
    }
    if (po->flags & HAS_ARG && !arg) {
        av_log(NULL, AV_LOG_ERROR, "Missing argument for option '%s'\n", opt);
        return AVERROR(EINVAL);
    }

    ret = write_option(gc, optctx, po, opt, arg);
    if (ret < 0)
        return ret;

    return !!(po->flags & HAS_ARG);
}

int parse_optgroup(GFFmpegContext *gc, void *optctx, OptionGroup *g)
{
    int i, ret;

    av_log(NULL, AV_LOG_DEBUG, "Parsing a group of options: %s %s.\n",
           g->group_def->name, g->arg);

    for (i = 0; i < g->nb_opts; i++) {
        Option *o = &g->opts[i];

        if (g->group_def->flags &&
            !(g->group_def->flags & o->opt->flags)) {
            av_log(NULL, AV_LOG_ERROR, "Option %s (%s) cannot be applied to "
                   "%s %s -- you are trying to apply an input option to an "
                   "output file or vice versa. Move this option before the "
                   "file it belongs to.\n", o->key, o->opt->help,
                   g->group_def->name, g->arg);
            return AVERROR(EINVAL);
        }

        av_log(NULL, AV_LOG_DEBUG, "Applying option %s (%s) with argument %s.\n",
               o->key, o->opt->help, o->val);

        ret = write_option(gc, optctx, o->opt, o->key, o->val);
        if (ret < 0)
            return ret;
    }

    av_log(NULL, AV_LOG_DEBUG, "Successfully parsed a group of options.\n");

    return 0;
}

int locate_option(int argc, char **argv, const OptionDef *options,
                  const char *optname)
{
    const OptionDef *po;
    int i;

    for (i = 1; i < argc; i++) {
        const char *cur_opt = argv[i];

        if (*cur_opt++ != '-')
            continue;

        po = find_option(options, cur_opt);
        if (!po->name && cur_opt[0] == 'n' && cur_opt[1] == 'o')
            po = find_option(options, cur_opt + 2);

        if ((!po->name && !strcmp(cur_opt, optname)) ||
             (po->name && !strcmp(optname, po->name)))
            return i;

        if (!po->name || po->flags & HAS_ARG)
            i++;
    }
    return 0;
}

static void check_options(const OptionDef *po)
{
    while (po->name) {
        if (po->flags & OPT_PERFILE)
            av_assert0(po->flags & (OPT_INPUT | OPT_OUTPUT));
        po++;
    }
}

void parse_loglevel(GFFmpegContext *gc, int argc, char **argv, const OptionDef *options)
{
    int idx = locate_option(argc, argv, options, "loglevel");
    const char *env;

    check_options(options);

    if (!idx)
        idx = locate_option(argc, argv, options, "v");
    if (idx && argv[idx + 1])
        opt_loglevel(gc, NULL, "loglevel", argv[idx + 1]);
}

static const AVOption *opt_find(void *obj, const char *name, const char *unit,
                            int opt_flags, int search_flags)
{
    const AVOption *o = av_opt_find(obj, name, unit, opt_flags, search_flags);
    if(o && !o->flags)
        return NULL;
    return o;
}

#define FLAGS (o->type == AV_OPT_TYPE_FLAGS && (arg[0]=='-' || arg[0]=='+')) ? AV_DICT_APPEND : 0
int opt_default(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg)
{
    const AVOption *o;
    int consumed = 0;
    char opt_stripped[128];
    const char *p;
    const AVClass *cc = avcodec_get_class(), *fc = avformat_get_class();
#if CONFIG_AVRESAMPLE
    const AVClass *rc = avresample_get_class();
#endif
#if CONFIG_SWSCALE
    const AVClass *sc = sws_get_class();
#endif
#if CONFIG_SWRESAMPLE
    const AVClass *swr_class = swr_get_class();
#endif

    if (!strcmp(opt, "debug") || !strcmp(opt, "fdebug"))
        av_log_set_level(AV_LOG_DEBUG);

    if (!(p = strchr(opt, ':')))
        p = opt + strlen(opt);
    av_strlcpy(opt_stripped, opt, FFMIN(sizeof(opt_stripped), p - opt + 1));

    if ((o = opt_find(&cc, opt_stripped, NULL, 0,
                         AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ)) ||
        ((opt[0] == 'v' || opt[0] == 'a' || opt[0] == 's') &&
         (o = opt_find(&cc, opt + 1, NULL, 0, AV_OPT_SEARCH_FAKE_OBJ)))) {
        av_dict_set(&gc->codec_opts, opt, arg, FLAGS);
        consumed = 1;
    }
    if ((o = opt_find(&fc, opt, NULL, 0,
                         AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        av_dict_set(&gc->format_opts, opt, arg, FLAGS);
        if (consumed)
            av_log(NULL, AV_LOG_VERBOSE, "Routing option %s to both codec and muxer layer\n", opt);
        consumed = 1;
    }
#if CONFIG_SWSCALE
    if (!consumed && (o = opt_find(&sc, opt, NULL, 0,
                         AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        struct SwsContext *sws = sws_alloc_context();
        int ret = av_opt_set(sws, opt, arg, 0);
        sws_freeContext(sws);
        if (!strcmp(opt, "srcw") || !strcmp(opt, "srch") ||
            !strcmp(opt, "dstw") || !strcmp(opt, "dsth") ||
            !strcmp(opt, "src_format") || !strcmp(opt, "dst_format")) {
            av_log(NULL, AV_LOG_ERROR, "Directly using swscale dimensions/format options is not supported, please use the -s or -pix_fmt options\n");
            return AVERROR(EINVAL);
        }
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error setting option %s.\n", opt);
            return ret;
        }

        av_dict_set(&gc->sws_dict, opt, arg, FLAGS);

        consumed = 1;
    }
#else
    if (!consumed && !strcmp(opt, "sws_flags")) {
        av_log(NULL, AV_LOG_WARNING, "Ignoring %s %s, due to disabled swscale\n", opt, arg);
        consumed = 1;
    }
#endif
#if CONFIG_SWRESAMPLE
    if (!consumed && (o=opt_find(&swr_class, opt, NULL, 0,
                                    AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        struct SwrContext *swr = swr_alloc();
        int ret = av_opt_set(swr, opt, arg, 0);
        swr_free(&swr);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error setting option %s.\n", opt);
            return ret;
        }
        av_dict_set(&gc->swr_opts, opt, arg, FLAGS);
        consumed = 1;
    }
#endif
#if CONFIG_AVRESAMPLE
    if ((o=opt_find(&rc, opt, NULL, 0,
                       AV_OPT_SEARCH_CHILDREN | AV_OPT_SEARCH_FAKE_OBJ))) {
        av_dict_set(&resample_opts, opt, arg, FLAGS);
        consumed = 1;
    }
#endif

    if (consumed)
        return 0;
    return AVERROR_OPTION_NOT_FOUND;
}

/*
 * Check whether given option is a group separator.
 *
 * @return index of the group definition that matched or -1 if none
 */
static int match_group_separator(const OptionGroupDef *groups, int nb_groups,
                                 const char *opt)
{
    int i;

    for (i = 0; i < nb_groups; i++) {
        const OptionGroupDef *p = &groups[i];
        if (p->sep && !strcmp(p->sep, opt))
            return i;
    }

    return -1;
}

/*
 * Finish parsing an option group.
 *
 * @param group_idx which group definition should this group belong to
 * @param arg argument of the group delimiting option
 */
static void finish_group(GFFmpegContext *gc, OptionParseContext *octx, int group_idx,
                         const char *arg)
{
    OptionGroupList *l = &octx->groups[group_idx];
    OptionGroup *g;

    GROW_ARRAY(gc, l->groups, l->nb_groups);
    g = &l->groups[l->nb_groups - 1];

    *g             = octx->cur_group;
    g->arg         = arg;
    g->group_def   = l->group_def;
    g->sws_dict    = gc->sws_dict;
    g->swr_opts    = gc->swr_opts;
    g->codec_opts  = gc->codec_opts;
    g->format_opts = gc->format_opts;
    g->resample_opts = gc->resample_opts;

    gc->codec_opts  = NULL;
    gc->format_opts = NULL;
    gc->resample_opts = NULL;
    gc->sws_dict    = NULL;
    gc->swr_opts    = NULL;
    init_opts(gc);

    memset(&octx->cur_group, 0, sizeof(octx->cur_group));
}

/*
 * Add an option instance to currently parsed group.
 */
static void add_opt(GFFmpegContext *gc, OptionParseContext *octx, const OptionDef *opt,
                    const char *key, const char *val)
{
    int global = !(opt->flags & (OPT_PERFILE | OPT_SPEC | OPT_OFFSET));
    OptionGroup *g = global ? &octx->global_opts : &octx->cur_group;

    GROW_ARRAY(gc, g->opts, g->nb_opts);
    g->opts[g->nb_opts - 1].opt = opt;
    g->opts[g->nb_opts - 1].key = key;
    g->opts[g->nb_opts - 1].val = val;
}

static void init_parse_context(GFFmpegContext *gc, OptionParseContext *octx,
                               const OptionGroupDef *groups, int nb_groups)
{
    static const OptionGroupDef global_group = { "global" };
    int i;

    memset(octx, 0, sizeof(*octx));

    octx->nb_groups = nb_groups;
    octx->groups    = av_mallocz_array(octx->nb_groups, sizeof(*octx->groups));
    if (!octx->groups)
        exit_program(gc, AVERROR(ENOMEM));

    for (i = 0; i < octx->nb_groups; i++)
        octx->groups[i].group_def = &groups[i];

    octx->global_opts.group_def = &global_group;
    octx->global_opts.arg       = "";

    init_opts(gc);
}

void uninit_parse_context(GFFmpegContext *gc, OptionParseContext *octx)
{
    int i, j;

    for (i = 0; i < octx->nb_groups; i++) {
        OptionGroupList *l = &octx->groups[i];

        for (j = 0; j < l->nb_groups; j++) {
            av_freep(&l->groups[j].opts);
            av_dict_free(&l->groups[j].codec_opts);
            av_dict_free(&l->groups[j].format_opts);
            av_dict_free(&l->groups[j].resample_opts);

            av_dict_free(&l->groups[j].sws_dict);
            av_dict_free(&l->groups[j].swr_opts);
        }
        av_freep(&l->groups);
    }
    av_freep(&octx->groups);

    av_freep(&octx->cur_group.opts);
    av_freep(&octx->global_opts.opts);

    uninit_opts(gc);
}

int split_commandline(GFFmpegContext *gc, OptionParseContext *octx, int argc, char *argv[],
                      const OptionDef *options,
                      const OptionGroupDef *groups, int nb_groups)
{
    int optindex = 1;
    int dashdash = -2;

    /* perform system-dependent conversions for arguments list */
    prepare_app_arguments(&argc, &argv);

    init_parse_context(gc, octx, groups, nb_groups);
    av_log(NULL, AV_LOG_DEBUG, "Splitting the commandline.\n");

    while (optindex < argc) {
        const char *opt = argv[optindex++], *arg;
        const OptionDef *po;
        int ret;

        av_log(NULL, AV_LOG_DEBUG, "Reading option '%s' ...", opt);

        if (opt[0] == '-' && opt[1] == '-' && !opt[2]) {
            dashdash = optindex;
            continue;
        }
        /* unnamed group separators, e.g. output filename */
        if (opt[0] != '-' || !opt[1] || dashdash+1 == optindex) {
            finish_group(gc, octx, 0, opt);
            av_log(NULL, AV_LOG_DEBUG, " matched as %s.\n", groups[0].name);
            continue;
        }
        opt++;

#define GET_ARG(arg)                                                           \
do {                                                                           \
    arg = argv[optindex++];                                                    \
    if (!arg) {                                                                \
        av_log(NULL, AV_LOG_ERROR, "Missing argument for option '%s'.\n", opt);\
        return AVERROR(EINVAL);                                                \
    }                                                                          \
} while (0)

        /* named group separators, e.g. -i */
        if ((ret = match_group_separator(groups, nb_groups, opt)) >= 0) {
            GET_ARG(arg);
            finish_group(gc, octx, ret, arg);
            av_log(NULL, AV_LOG_DEBUG, " matched as %s with argument '%s'.\n",
                   groups[ret].name, arg);
            continue;
        }

        /* normal options */
        po = find_option(options, opt);
        if (po->name) {
            if (po->flags & OPT_EXIT) {
                /* optional argument, e.g. -h */
                arg = argv[optindex++];
            } else if (po->flags & HAS_ARG) {
                GET_ARG(arg);
            } else {
                arg = "1";
            }

            add_opt(gc, octx, po, opt, arg);
            av_log(NULL, AV_LOG_DEBUG, " matched as option '%s' (%s) with "
                   "argument '%s'.\n", po->name, po->help, arg);
            continue;
        }

        /* AVOptions */
        if (argv[optindex]) {
            ret = opt_default(gc, NULL, opt, argv[optindex]);
            if (ret >= 0) {
                av_log(NULL, AV_LOG_DEBUG, " matched as AVOption '%s' with "
                       "argument '%s'.\n", opt, argv[optindex]);
                optindex++;
                continue;
            } else if (ret != AVERROR_OPTION_NOT_FOUND) {
                av_log(NULL, AV_LOG_ERROR, "Error parsing option '%s' "
                       "with argument '%s'.\n", opt, argv[optindex]);
                return ret;
            }
        }

        /* boolean -nofoo options */
        if (opt[0] == 'n' && opt[1] == 'o' &&
            (po = find_option(options, opt + 2)) &&
            po->name && po->flags & OPT_BOOL) {
            add_opt(gc, octx, po, opt, "0");
            av_log(NULL, AV_LOG_DEBUG, " matched as option '%s' (%s) with "
                   "argument 0.\n", po->name, po->help);
            continue;
        }

        av_log(NULL, AV_LOG_ERROR, "Unrecognized option '%s'.\n", opt);
        return AVERROR_OPTION_NOT_FOUND;
    }

    if (octx->cur_group.nb_opts || gc->codec_opts || gc->format_opts || gc->resample_opts)
        av_log(NULL, AV_LOG_WARNING, "Trailing option(s) found in the "
               "command: may be ignored.\n");

    av_log(NULL, AV_LOG_DEBUG, "Finished splitting the commandline.\n");

    return 0;
}

int opt_cpuflags(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg)
{
    int ret;
    unsigned flags = av_get_cpu_flags();

    if ((ret = av_parse_cpu_caps(&flags, arg)) < 0)
        return ret;

    av_force_cpu_flags(flags);
    return 0;
}

int opt_loglevel(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg)
{
    const struct { const char *name; int level; } log_levels[] = {
        { "quiet"  , AV_LOG_QUIET   },
        { "panic"  , AV_LOG_PANIC   },
        { "fatal"  , AV_LOG_FATAL   },
        { "error"  , AV_LOG_ERROR   },
        { "warning", AV_LOG_WARNING },
        { "info"   , AV_LOG_INFO    },
        { "verbose", AV_LOG_VERBOSE },
        { "debug"  , AV_LOG_DEBUG   },
        { "trace"  , AV_LOG_TRACE   },
    };
    const char *token;
    char *tail;
    int flags = av_log_get_flags();
    int level = av_log_get_level();
    int cmd, i = 0;

    av_assert0(arg);
    while (*arg) {
        token = arg;
        if (*token == '+' || *token == '-') {
            cmd = *token++;
        } else {
            cmd = 0;
        }
        if (!i && !cmd) {
            flags = 0;  /* missing relative prefix, build absolute value */
        }
        if (!strncmp(token, "repeat", 6)) {
            if (cmd == '-') {
                flags |= AV_LOG_SKIP_REPEATED;
            } else {
                flags &= ~AV_LOG_SKIP_REPEATED;
            }
            arg = token + 6;
        } else if (!strncmp(token, "level", 5)) {
            if (cmd == '-') {
                flags &= ~AV_LOG_PRINT_LEVEL;
            } else {
                flags |= AV_LOG_PRINT_LEVEL;
            }
            arg = token + 5;
        } else {
            break;
        }
        i++;
    }
    if (!*arg) {
        goto end;
    } else if (*arg == '+') {
        arg++;
    } else if (!i) {
        flags = av_log_get_flags();  /* level value without prefix, reset flags */
    }

    for (i = 0; i < FF_ARRAY_ELEMS(log_levels); i++) {
        if (!strcmp(log_levels[i].name, arg)) {
            level = log_levels[i].level;
            goto end;
        }
    }

    level = strtol(arg, &tail, 10);
    if (*tail) {
        av_log(NULL, AV_LOG_FATAL, "Invalid loglevel \"%s\". "
               "Possible levels are numbers or:\n", arg);
        for (i = 0; i < FF_ARRAY_ELEMS(log_levels); i++)
            av_log(NULL, AV_LOG_FATAL, "\"%s\"\n", log_levels[i].name);
        exit_program(gc, 1);
    }

end:
    av_log_set_flags(flags);
    av_log_set_level(level);
    return 0;
}

int opt_max_alloc(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg)
{
    char *tail;
    size_t max;

    max = strtol(arg, &tail, 10);
    if (*tail) {
        av_log(NULL, AV_LOG_FATAL, "Invalid max_alloc \"%s\".\n", arg);
        exit_program(gc, 1);
    }
    av_max_alloc(max);
    return 0;
}

int opt_timelimit(GFFmpegContext *gc, void *optctx, const char *opt, const char *arg)
{
#if HAVE_SETRLIMIT
    int lim = parse_number_or_die(gc, opt, arg, OPT_INT64, 0, INT_MAX);
    struct rlimit rl = { lim, lim + 1 };
    if (setrlimit(RLIMIT_CPU, &rl))
        perror("setrlimit");
#else
    av_log(NULL, AV_LOG_WARNING, "-%s not implemented on this OS\n", opt);
#endif
    return 0;
}

void print_error(const char *filename, int err)
{
    char errbuf[128];
    const char *errbuf_ptr = errbuf;

    if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
        errbuf_ptr = strerror(AVUNERROR(err));
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}

static int warned_cfg = 0;

#define INDENT        1
#define SHOW_VERSION  2
#define SHOW_COPYRIGHT 8

#define PRINT_CODEC_SUPPORTED(codec, field, type, list_name, term, get_name) \
    if (codec->field) {                                                      \
        const type *p = codec->field;                                        \
                                                                             \
        printf("    Supported " list_name ":");                              \
        while (*p != term) {                                                 \
            get_name(*p);                                                    \
            printf(" %s", name);                                             \
            p++;                                                             \
        }                                                                    \
        printf("\n");                                                        \
    }                                                                        \


static char get_media_type_char(enum AVMediaType type)
{
    switch (type) {
        case AVMEDIA_TYPE_VIDEO:    return 'V';
        case AVMEDIA_TYPE_AUDIO:    return 'A';
        case AVMEDIA_TYPE_DATA:     return 'D';
        case AVMEDIA_TYPE_SUBTITLE: return 'S';
        case AVMEDIA_TYPE_ATTACHMENT:return 'T';
        default:                    return '?';
    }
}


int read_yesno(void)
{
    int c = getchar();
    int yesno = (av_toupper(c) == 'Y');

    while (c != '\n' && c != EOF)
        c = getchar();

    return yesno;
}

FILE *get_preset_file(char *filename, size_t filename_size,
                      const char *preset_name, int is_path,
                      const char *codec_name)
{
    FILE *f = NULL;
    int i;
    const char *base[3] = { getenv("FFMPEG_DATADIR"),
                            getenv("HOME"),
                            FFMPEG_DATADIR, };

    if (is_path) {
        av_strlcpy(filename, preset_name, filename_size);
        f = fopen(filename, "r");
    } else {
#if HAVE_GETMODULEHANDLE && defined(_WIN32)
        char datadir[MAX_PATH], *ls;
        base[2] = NULL;

        if (GetModuleFileNameA(GetModuleHandleA(NULL), datadir, sizeof(datadir) - 1))
        {
            for (ls = datadir; ls < datadir + strlen(datadir); ls++)
                if (*ls == '\\') *ls = '/';

            if (ls = strrchr(datadir, '/'))
            {
                *ls = 0;
                strncat(datadir, "/ffpresets",  sizeof(datadir) - 1 - strlen(datadir));
                base[2] = datadir;
            }
        }
#endif
        for (i = 0; i < 3 && !f; i++) {
            if (!base[i])
                continue;
            snprintf(filename, filename_size, "%s%s/%s.ffpreset", base[i],
                     i != 1 ? "" : "/.ffmpeg", preset_name);
            f = fopen(filename, "r");
            if (!f && codec_name) {
                snprintf(filename, filename_size,
                         "%s%s/%s-%s.ffpreset",
                         base[i], i != 1 ? "" : "/.ffmpeg", codec_name,
                         preset_name);
                f = fopen(filename, "r");
            }
        }
    }

    return f;
}

int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
    int ret = avformat_match_stream_specifier(s, st, spec);
    if (ret < 0)
        av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
    return ret;
}

static const char *gpu_decoder_name_suffix[]  = {"_cuvid", "_nvdec", "_mmal", NULL};
static const char *gpu_encoder_name_suffix[]  = {"_cuvid", "_nvenc", "_omx", NULL};

static AVCodec *_try_auto_use_gpu_by_name(GFFmpegContext *gc, char *name, int is_encoder) {
    char name_buf[256] = {0};
    int i;

    if (gc->auto_use_gpu && name) {
        AVCodec *codec = NULL;
        if (is_encoder) {
            for (i = 0; gpu_encoder_name_suffix[i] != NULL; i++) {
                strcpy(name_buf, name);
                strcat(name_buf, gpu_encoder_name_suffix[i]);
                codec = avcodec_find_encoder_by_name(name_buf);
                if (codec) {
                    gc->gpu_auto_used = 1;
                    gc->gpu_encoder_name = codec->name;
                    av_log(NULL, AV_LOG_INFO, "Try to use gpu to encoder: %s", codec->name);
                    return codec;
                }
            }
        } else {
            for (i = 0; gpu_decoder_name_suffix[i] != NULL; i++) {
                strcpy(name_buf, name);
                strcat(name_buf, gpu_decoder_name_suffix[i]);
                codec = avcodec_find_decoder_by_name(name_buf);
                if (codec) {
                    gc->gpu_auto_used = 1;
                    gc->gpu_decoder_name = codec->name;
                    av_log(NULL, AV_LOG_INFO, "Try to use gpu to decoder: %s", codec->name);
                    return codec;
                }
            }
        }
    }

    return NULL;
}

AVCodec *try_auto_use_gpu_by_name(GFFmpegContext *gc, char *name, int is_encoder) {

    AVCodec *codec = is_encoder ? avcodec_find_encoder_by_name(name)
                                : avcodec_find_decoder_by_name(name);

    if (gc->auto_use_gpu && codec) {
        AVCodec *tmp_codec = _try_auto_use_gpu_by_name(gc, codec->name, is_encoder);
        if (tmp_codec) {
            return tmp_codec;
        }
    }

    return codec;
}

AVCodec *try_auto_use_gpu_by_codec_id(GFFmpegContext *gc, enum AVCodecID codec_id, int is_encoder) {

    AVCodec *codec = is_encoder ? avcodec_find_encoder(codec_id)
                                : avcodec_find_decoder(codec_id);

    if (gc->auto_use_gpu && codec) {
        AVCodec *tmp_codec = _try_auto_use_gpu_by_name(gc, codec->name, is_encoder);
        if (tmp_codec) {
            return tmp_codec;
        }
    }

    return codec;
}

AVDictionary *filter_codec_opts(GFFmpegContext *gc, AVDictionary *opts, enum AVCodecID codec_id,
                                AVFormatContext *s, AVStream *st, AVCodec *codec)
{
    AVDictionary    *ret = NULL;
    AVDictionaryEntry *t = NULL;
    int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
                                      : AV_OPT_FLAG_DECODING_PARAM;
    char          prefix = 0;
    const AVClass    *cc = avcodec_get_class();

    if (!codec) {
        codec = try_auto_use_gpu_by_codec_id(gc, codec_id, s->oformat);
    }

    switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
        prefix  = 'v';
        flags  |= AV_OPT_FLAG_VIDEO_PARAM;
        break;
    case AVMEDIA_TYPE_AUDIO:
        prefix  = 'a';
        flags  |= AV_OPT_FLAG_AUDIO_PARAM;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        prefix  = 's';
        flags  |= AV_OPT_FLAG_SUBTITLE_PARAM;
        break;
    }

    while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
        char *p = strchr(t->key, ':');

        /* check stream specification in opt name */
        if (p)
            switch (check_stream_specifier(s, st, p + 1)) {
            case  1: *p = 0; break;
            case  0:         continue;
            default:         exit_program(gc, 1);
            }

        if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
            !codec ||
            (codec->priv_class &&
             av_opt_find(&codec->priv_class, t->key, NULL, flags,
                         AV_OPT_SEARCH_FAKE_OBJ)))
            av_dict_set(&ret, t->key, t->value, 0);
        else if (t->key[0] == prefix &&
                 av_opt_find(&cc, t->key + 1, NULL, flags,
                             AV_OPT_SEARCH_FAKE_OBJ))
            av_dict_set(&ret, t->key + 1, t->value, 0);

        if (p)
            *p = ':';
    }
    return ret;
}

AVDictionary **setup_find_stream_info_opts(GFFmpegContext *gc, AVFormatContext *s,
                                           AVDictionary *codec_opts)
{
    int i;
    AVDictionary **opts;

    if (!s->nb_streams)
        return NULL;
    opts = av_mallocz_array(s->nb_streams, sizeof(*opts));
    if (!opts) {
        av_log(NULL, AV_LOG_ERROR,
               "Could not alloc memory for stream options.\n");
        return NULL;
    }
    for (i = 0; i < s->nb_streams; i++)
        opts[i] = filter_codec_opts(gc, codec_opts, s->streams[i]->codecpar->codec_id,
                                    s, s->streams[i], NULL);
    return opts;
}

void *grow_array(GFFmpegContext *gc, void *array, int elem_size, int *size, int new_size)
{
    if (new_size >= INT_MAX / elem_size) {
        av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
        exit_program(gc, 1);
    }
    if (*size < new_size) {
        uint8_t *tmp = av_realloc_array(array, new_size, elem_size);
        if (!tmp) {
            av_log(NULL, AV_LOG_ERROR, "Could not alloc buffer.\n");
            exit_program(gc, AVERROR(ENOMEM));
        }
        memset(tmp + *size*elem_size, 0, (new_size-*size) * elem_size);
        *size = new_size;
        return tmp;
    }
    return array;
}

double get_rotation(AVStream *st)
{
    uint8_t* displaymatrix = av_stream_get_side_data(st,
                                                     AV_PKT_DATA_DISPLAYMATRIX, NULL);
    double theta = 0;
    if (displaymatrix)
        theta = -av_display_rotation_get((int32_t*) displaymatrix);

    theta -= 360*floor(theta/360 + 0.9/360);

    if (fabs(theta - 90*round(theta/90)) > 2)
        av_log(NULL, AV_LOG_WARNING, "Odd rotation angle.\n"
               "If you want to help, upload a sample "
               "of this file to https://streams.videolan.org/upload/ "
               "and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)");

    return theta;
}

void parse_options(GFFmpegContext *gc, void *optctx, int argc, char **argv, const OptionDef *options,
                   void (*parse_arg_function)(GFFmpegContext *, void *, const char*))
{
    const char *opt;
    int optindex, handleoptions = 1, ret;

    /* perform system-dependent conversions for arguments list */
    prepare_app_arguments(&argc, &argv);

    /* parse options */
    optindex = 1;
    while (optindex < argc) {
        opt = argv[optindex++];

        if (handleoptions && opt[0] == '-' && opt[1] != '\0') {
            if (opt[1] == '-' && opt[2] == '\0') {
                handleoptions = 0;
                continue;
            }
            opt++;

            if ((ret = parse_option(gc, optctx, opt, argv[optindex], options)) < 0)
                exit_program(gc, ret);
            optindex += ret;
        } else {
            if (parse_arg_function)
                parse_arg_function(gc, optctx, opt);
        }
    }
}

//static int decode_interrupt_cb(GFFmpegContext *gc)
//{
//    return gc->received_nb_signals > atomic_load(&gc->transcode_init_done);
//}

static const AVClass g_ffmpeg_context_class = {
        .class_name              = "G_FFMPEG_CONTEXT",
        .version                 = 0x88888888,
        .log_level_offset_offset = offsetof(GFFmpegContext, log_level_offset),
        .category                = G_FFMPEG_CONTEXT_CATEGORY,
};

GFFmpegContext * g_ffmpeg_context_init() {
    GFFmpegContext *gc = av_mallocz(sizeof(GFFmpegContext));
    gc->class = &g_ffmpeg_context_class;

    gc->audio_drift_threshold = 0.1;
    gc->dts_delta_threshold   = 10;
    gc->dts_error_threshold   = 3600*30;

    gc->audio_volume      = 256;
    gc->audio_sync_method = 0;
    gc->video_sync_method = VSYNC_AUTO;
    gc->frame_drop_threshold = 0;

    gc->do_deinterlace    = 0;
    gc->do_benchmark      = 0;
    gc->do_benchmark_all  = 0;
    gc->do_hex_dump       = 0;
    gc->do_pkt_dump       = 0;

    gc->copy_ts           = 0;
    gc->start_at_zero     = 0;
    gc->copy_tb           = -1;
    gc->debug_ts          = 0;
    gc->exit_on_error     = 0;
    gc->abort_on_flags    = 0;
    gc->print_stats       = -1;
    gc->qp_hist           = 0;
    gc->stdin_interaction = 1;
    gc->frame_bits_per_raw_sample = 0;

    gc->max_error_rate  = 2.0/3;
    gc->filter_nbthreads = 0;
    gc->filter_complex_nbthreads = 0;
    gc->vstats_version = 2;

//    gc->int_cb.callback = decode_interrupt_cb;
//    gc->int_cb.opaque = gc;

    gc->run_as_daemon  = 0;
    gc->nb_frames_dup = 0;
    gc->dup_warning = 1000;
    gc->nb_frames_drop = 0;
    gc->want_sdp = 1;

    gc->execute_terminated = 0;
    gc->received_nb_signals = 0;
    gc->ffmpeg_exited = 0;
    gc->main_return_code = 0;

    gc->intra_only         = 0;
    gc->file_overwrite     = 0;
    gc->no_file_overwrite  = 0;
    gc->do_psnr            = 0;
    gc->input_sync;
    gc->input_stream_potentially_available = 0;
    gc->ignore_unknown_streams = 0;
    gc->copy_unknown_streams = 0;
    gc->find_stream_info = 1;

    // probe need
    gc->show_private_data = 1;

    // add for gfg golang
    gc->write_packet = 1;

    // GPU
    gc->auto_use_gpu = 0;
    gc->gpu_auto_used = 0;

    return gc;
}

static int setargs(char *args, char **argv)
{
    int count = 0;

    while (isspace(*args)) ++args;
    while (*args) {
        if (argv) argv[count] = args;
        while (*args && !isspace(*args)) ++args;
        if (argv && *args) *args++ = '\0';
        while (isspace(*args)) ++args;
        count++;
    }
    return count;
}

char **parsedargs(char *args, int *argc)
{
    char **argv = NULL;
    int    argn = 0;

    if (args && *args
        && (args = strdup(args))
        && (argn = setargs(args,NULL))
        && (argv = malloc((argn+1) * sizeof(char *)))) {
        *argv++ = args;
        argn = setargs(args,argv);
    }

    if (args && !argv) free(args);

    *argc = argn;
    return argv;
}

void freeparsedargs(char **argv)
{
    if (argv) {
        free(argv[-1]);
        free(argv-1);
    }
}

int enter_program(GFFmpegContext *gc, int argc, char **argv, int (*main_func)(GFFmpegContext *gc, int argc, char **argv)) {
    int ret;

    ret = setjmp(gc->_jmp_buf);
    switch (ret) {
        case 0:
            if (main_func) {
                ret = main_func(gc, argc, argv);
            }
            break;
        case JUMP_BUFFER_SUCCESS:
            ret = 0;
            break;
        default:
            break;
    }
    av_log(NULL, AV_LOG_INFO, "main_func returned with code: %d\n", ret);
    return ret;
}