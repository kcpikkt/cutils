/*
 * The MIT License (MIT)
 *
 *  Copyright (c) Kacper Kokot
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 * Option parsing because getopt.h is absolutely rubbish.
 *
 * Example usage;
 *
 *   $ cat main.c
 *
 *   #include <stdio.h>
 *   #define KK_ARR_IMPL
 *   #include "arr.h"
 *   #define KK_OPTS_IMPL
 *   #include "opts.h"
 *
 *   enum { O_THREE, O_SEVEN, O_ELEVEN };
 *
 *   struct opt opts[] = { 
 *     [O_THREE] = { "three", "t", OPT_NOPARAM,
 *       .desc = "switch option, doesn't take any params."
 *     },
 *     [O_SEVEN] = { "seven", "s", OPT_INTEGER,
 *       .desc = "option taking one integer parameter."
 *     },
 *     [O_ELEVEN] = { "eleven", NULL, OPT_MULPARAM,
 *       .desc = "option taking multiple string params."
 *     },
 *     { NULL } 
 *   };
 *   
 *   int main (int argc, char **argv)
 *   {
 *     if(opts_parse(opts, argc, argv, 0))
 *       goto exit;
 *   
 *     printf("option three is %s\n",
 *        opts_get(&opts[O_THREE], NULL) ? "set" : "not set");
 *   
 *     int seven_arg;
 *     if(opts_get(&opts[O_SEVEN], &seven_arg))
 *       printf("seven is set to %d\n", seven_arg);
 *   
 *     struct arr *eleven_params;
 *     if(opts_get(&opts[O_ELEVEN], &eleven_params)) {
 *       printf("eleven params are");
 *       arr_for(const char *, param, eleven_params) {
 *         printf(" '%s'", *param);
 *       }
 *     }
 *   }
 *
 *   $ gcc main.c && ./a.out --three --eleven 1 2 2 -t -s 1
 *
 */

#ifndef _KK_OPTS_H_
#define _KK_OPTS_H_

#include "arr.h"
#include <stdint.h>

#if !defined(NDEBUG)
# include <stdio.h>
# include <assert.h>
# define OPTS_ASSERT(COND) assert(COND)
#else  
# define OPTS_ASSERT(...)
#endif

enum {
  OPT_REQUIRED      = (1 << 0),
  OPT_NOPARAM       = (1 << 1),
  OPT_MULPARAM      = (1 << 2),
  OPT_INTEGER       = (1 << 3),
  OPT_OPTPARAM      = (1 << 4),
  OPT_FLOAT         = (1 << 5),
  OPT_DEFAULT       = (1 << 6),
  OPT_RANGE         = (1 << 7),
};

enum {
  OPT_PARSE_NOFAIL  = (1 << 0),
  OPT_PARSE_SHIFT   = (1 << 1),
};

typedef int (opt_parse_proc)(char *arg, void *val);

struct opt {
/* set by the user */
  const char *name;
  const char *sname;
  uint8_t flags;
  opt_parse_proc *parse;
  const char *desc;

  union {
    const char *default_string;
    int default_int;
    float default_float;
  };

  union {
    struct {
      float min;
      float max;
    } range_float;

    struct {
      int min;
      int max;
    } range_int;
  };

/* set by the api */
  bool set;
  struct arr params;
};

int opts_parse(struct opt *opts, int argc, char **argv, uint8_t flags);

/* 
 * This function cannot be called for options with OPT_MULPARAM set,
 * in which case you have to manually traverse parameters by e.g.
 *
 * arr_for(const char*, param, &opts[MYOPT].params)
 *   printf("%s", *param);
 */
bool opts_get(struct opt *opt, void *ret);

void opts_help(struct opt *opts, FILE *stream);

void opts_print(struct opt *opts, FILE *stream);

#endif /* _KK_OPTS_H_ */

/* impl */

#ifdef KK_OPTS_IMPL

#ifndef _KK_OPTS_IMPL_
#define _KK_OPTS_IMPL_

#include <errno.h> 
#include <string.h> 

/* only internal, no reason for user to use this */
#define _opts_for(OPT, OPTS) \
  for(struct opt *(OPT) = (OPTS); (OPT) && (OPT)->name; ++(OPT))

static inline
const char *_get_opt_name(const char *arg)
{
  size_t len = strlen(arg);
  if(len >= 3 && arg[0] == '-' && arg[1] == '-' && arg[2] != '-')
    return (arg + 2);
  return NULL;
}

const char *_get_opt_sname(const char *arg) 
{
  size_t len = strlen(arg);
  if(len == 2 && arg[0] == '-' && arg[1] != '-')
    return (arg + 1);
  return NULL;
}

static struct opt * opt_match(struct opt *opts, const char *arg) 
{
  const char *opt_name = _get_opt_name(arg);
  const char *opt_sname = _get_opt_sname(arg);
  
  if(!opt_name && !opt_sname)
    return NULL;
  
  while(opts->name) {
    if(opt_name && !strcmp(opts->name, opt_name))
      return opts;

    if(opt_sname && opts->sname && !strcmp(opts->sname, opt_sname))
      return opts;

    opts++;
  }
  return NULL;
}

/* TODO: leading whitespace */
static inline bool _is_opt(char *arg) 
{
  if(!arg)
    return false;

  size_t len = strlen(arg);

  if(len >= 2 && arg[0] == '-' && arg[1] != '-')
    return true;

  //if(len >= 3 && arg[0] == '-' && arg[1] == '-' && arg[2] != '-')
  if(_get_opt_name(arg))
    return true;

  return false;
}

static inline bool opt_is_int(struct opt *opt) 
{ 
  return opt->flags & OPT_INTEGER; 
}

static inline bool opt_is_float(struct opt *opt) 
{ 
  return opt->flags & OPT_FLOAT; 
}

static inline 
size_t _opt_param_size(struct opt *opt) 
{
  if(opt_is_int(opt))
    return sizeof(int);
  else if(opt_is_float(opt))
    return sizeof(float);

  return sizeof(const char *);
}

static
int opts_set(struct opt *opt, char *arg) 
{
  int ret = -1;
  char *nptr;

  int iarg;
  float farg;
  void *parg;

  assert(arg);

  parg = opt_is_int(opt) ? (void*)&iarg 
    : (opt_is_float(opt) ? (void*)&farg 
    :                      (void*)&arg);

  /*  */ if(opt->parse) {
    if(opt->parse(arg, parg)) {
      fprintf(stderr, 
          "Couldn't parse argument '%s' to %s/%s option.\n", 
          arg, opt->name, opt->sname ? opt->sname : "");
      goto exit;
    }

  } else if(opt_is_int(opt)) {
    iarg = strtol(arg, &nptr, 10);
    if(nptr == arg || errno != 0) {
      fprintf(stderr, 
          "Invalid argument '%s' to %s/%s option - expected an integer.\n", 
          arg, opt->name, opt->sname ? opt->sname : "");
      goto exit;
    }
  } else if(opt_is_float(opt)) {
    farg = strtof(arg, &nptr);
    if(nptr == arg || errno != 0) {
      fprintf(stderr, 
          "Invalid argument '%s' to %s/%s option - expected a float.\n", 
          arg, opt->name, opt->sname ? opt->sname : "");
      goto exit;
    }
    if(opt->flags & OPT_RANGE) {
      if(!(opt->range_float.min <= farg && farg <= opt->range_float.max)) {
        fprintf(stderr, 
            "Invalid argument '%.2f' to %s/%s option - "
            "must be in range [%.2f-%.2f].\n", 
            farg, opt->name, opt->sname ? opt->sname : "", 
            opt->range_float.min, opt->range_float.max);
        goto exit;
      }
    }
  } 

  assert(opt->params.esz == _opt_param_size(opt));
  arr_push(&opt->params, parg);

  ret = 0;
exit:
  return ret;
}


#if OPTS_DEBUG
static inline
void _opts_debug_print(struct opt *opts) 
{
  while(opts->name) {
    fprintf(stderr, "--%s%s", opts->name, opts->set ? ", set" : ", not set");

    if(opts->set && !(opts->flags & OPT_NOPARAM)) {
      fprintf(stderr, ", ");

      /**/ if(opt_is_int(opts))   fprintf(stderr, "integer");
      else if(opt_is_float(opts)) fprintf(stderr, "float");
      else                        fprintf(stderr, "string");
        
      fprintf(stderr, " params:");

      arr_for(void, param, &opts->params) {
        /**/ if(opt_is_int(opts))
          fprintf(stderr, " %d", *(int*)param);
        else if(opt_is_float(opts))
          fprintf(stderr, " %f", *(float*)param);
        else
          fprintf(stderr, " '%s'", *(const char**)param);
      }
    }
    fprintf(stderr, "\n");
    opts++;
  }
}
#endif

int _opts_cmdline_shift(int argc, char **argv, int n)
{
  for(int i = 0; i < argc; ++i) {
    if(i >= n)
      argv[i - n] = argv[i];
    argv[i] = NULL;
  }
  return n;
}

void *_opt_default_param(struct opt *opt) 
{
  /**/ if(opt_is_int(opt)) 
    return (void*)&opt->default_int;
  else if(opt_is_float(opt))
    return (void*)&opt->default_float;

  return (void*)&opt->default_string;
}

static inline
int opts_precheck(struct opt *opts)
{
  _opts_for(opt, opts) {
    if(opt->flags & OPT_MULPARAM && opt->flags & OPT_DEFAULT) {
      fprintf(stderr, "Option flags OPT_MULPARAM and OPT_DEFAULT"
          "are not supported in conjuction\n"); /* we could but oof */
      return -1;
    }
  }
  return 0;
}

static inline 
int opts_postcheck(struct opt *opts) 
{
  _opts_for(opt, opts) {
    if(!(opt->flags & OPT_MULPARAM) && opt->params.cnt > 1) {
      return -1;
    }
  }
  return 0;
}

int opts_parse(struct opt *opts, int argc, char **argv, uint8_t flags)
{
  int ret = -1;
  struct opt *opt = NULL;

  if(opts_precheck(opts))
    goto exit;

#if OPTS_DEBUG
  fprintf(stderr, "%s cmdline:", __func__);
  for(int i = 0; i < argc; ++i)
    fprintf(stderr, "'%s' ", argv[i]);
  fprintf(stderr, "\n");
#endif

  int i = 0, s = 0;
  for(int i = 0; i <= argc; ++i) {
    char *arg = i < argc ? argv[i - s] : NULL;

#if OPTS_DEBUG
    fprintf(stderr, "%s %10s %10s\n", __func__,
        opt ? opt->name : "(null)", arg);
#endif

    if(opt) {
      if(opt->flags & OPT_NOPARAM)
        goto parse_as_opt;

      if(!arr_alive(&opt->params))
        arr_init(&opt->params, _opt_param_size(opt));

      if(!arg || _is_opt(arg)) {
        if(opt->flags & OPT_MULPARAM && !arr_empty(&opt->params))
          goto parse_as_opt;

        if(opt->flags & OPT_OPTPARAM)
          goto parse_as_opt;

        fprintf(stderr, 
            "Option %s%s%s requires an argument\n", 
            opt->name, 
            opt->sname ? "/-" : "",
            opt->sname ? opt->sname : "");
        goto exit;
      }

      if(arg) {
        if(opts_set(opt, arg))
          goto exit;

        opt = (opt->flags & OPT_MULPARAM) ? opt : NULL;
      }
      /* parsed an option parameter */

      if(flags & OPT_PARSE_SHIFT)
        s += _opts_cmdline_shift(argc - i, &argv[i - s], 1);
      continue;
    }

parse_as_opt:
    opt = NULL;
    assert(!opt);

    if(!arg) 
      break;

    if(_is_opt(arg)) {
      if((opt = opt_match(opts, arg)) && (opt->set = true)) {
        /* parsed an option parameter */
        arg = NULL;

        if(flags & OPT_PARSE_SHIFT)
          s += _opts_cmdline_shift(argc - i, &argv[i - s], 1);
        continue;
      }
    }

    if(!(flags & OPT_PARSE_NOFAIL)) {
      fprintf(stderr, "Unrecognised option '%s'.\n", arg);
      goto exit;
    }
  }

  for(struct opt *opt = opts; opt->name; ++opt) {
    if(!opt->set && (opt->flags & OPT_DEFAULT)) {
      opt->set = true;
      if(arr_init(&opt->params, _opt_param_size(opt)))
        goto exit;
      arr_push(&opt->params, _opt_default_param(opt));
    }
  }

#if OPTS_DEBUG
  if(flags & OPT_PARSE_SHIFT) {
    fprintf(stderr, "%s cmdline after shift:", __func__);
    for(int i = 0; i < argc - s; ++i)
      fprintf(stderr, "'%s' ", argv[i]);
    fprintf(stderr, "\n");
  }

  _opts_debug_print(opts);
#endif

  OPTS_ASSERT(opts_postcheck(opts) == 0);
  ret = 0;
exit:
  return ret;
}

bool opts_get(struct opt *opt, void *ret) 
{
  if(!opt->set) 
    return false;

  if(ret) {
    /*  */ if(opt->flags & OPT_NOPARAM) {
      *(bool*)ret = true;
    } else if(opt->flags & OPT_MULPARAM)  { 
        *(struct arr**)ret = &opt->params;
    } else {
      assert(opt->params.cnt == 1);

      if(opt_is_int(opt))
        *(int*)ret = *arr_att(int, &opt->params, 0);
      else if(opt_is_float(opt))
        *(float*)ret = *arr_att(float, &opt->params, 0);
      else
        *(char**)ret = *arr_att(char*, &opt->params, 0);
    }
  }

  return true;
}

bool opts_help_opt_range(struct opt *opt, FILE *stream) 
{
  if(opt->flags & OPT_RANGE) {
    if(opt_is_int(opt)) {
      fprintf(stream, "range [%d-%d]", 
          opt->range_int.min, opt->range_int.max);

    } else if(opt_is_float(opt)) {
      fprintf(stream, "range [%.2f-%.2f]", 
          opt->range_float.min, opt->range_float.max);

    } else {
      assert(0);
    }
    return true;
  }
  return false;
}

bool opts_help_opt_default(struct opt *opt, FILE *stream) 
{
  if(opt->flags & OPT_DEFAULT) {
    if(opt_is_int(opt)) {
      fprintf(stream, "default %d", opt->default_int);

    } else if(opt_is_float(opt)) {
      fprintf(stream, "default %.2f", opt->default_float);

    } else {
      fprintf(stream, "default '%s'", opt->default_string);
    }
    return true;
  }
  return false;
}

void opts_help_opt_arg(struct opt *opt, FILE *stream) 
{
  size_t bufsz = 13, bufoff = 0;
  char buf[bufsz + 1];
  memset(buf, '\0', bufsz + 1);

  if(opt->flags & OPT_NOPARAM)
    goto exit;

  bufoff += snprintf(buf + bufoff, bufsz - bufoff, "%s",
      opt->flags & OPT_OPTPARAM ? " [" : " <");

  if(opt_is_int(opt)) 
    bufoff += snprintf(buf + bufoff, bufsz - bufoff, "integer");
  else if(opt_is_float(opt)) 
    bufoff += snprintf(buf + bufoff, bufsz - bufoff, "float");
  else
    bufoff += snprintf(buf + bufoff, bufsz - bufoff, "arg");

  if(opt->flags & OPT_MULPARAM)
    bufoff += 
      snprintf(buf + bufoff, bufsz - bufoff, "...");

  bufoff += snprintf(buf + bufoff, bufsz - bufoff, "%s",
        opt->flags & OPT_OPTPARAM ? "] " : "> ");

exit:
  memset(buf + bufoff, ' ', bufsz - bufoff);
  fprintf(stream, "%s", buf);
}

void opts_help(struct opt *opts, FILE *stream) 
{
  size_t head_len = 0, len = 0;

  _opts_for(opt, opts) {
    len = strlen(opt->name);
    if(len > head_len)
      head_len = len;
    opt++;
  }

  _opts_for(opt, opts) {
    fprintf(stream, "  %s%c %s", 
        opt->sname ? opt->sname : "  ", 
        opt->sname ? ',' : ' ', 
        opt->name);

    opts_help_opt_arg(opt, stream);

    len = head_len - strlen(opt->name);
    for(size_t i = 0; i < len; ++i)
      fprintf(stream, " ");

    fprintf(stream, "%s", opt->desc ? opt->desc : "");

    bool prev = false;
    if(opt->flags & OPT_DEFAULT || opt->flags & OPT_RANGE) {
      fprintf(stream, opt->desc ? " (" : "(");
      prev = opts_help_opt_range(opt, stream);
      if(prev) fprintf(stream, ", ");
      prev = opts_help_opt_default(opt, stream);
      fprintf(stream, ")");
    }

    fprintf(stream, "\n");
  }
}

void opts_print(struct opt *opts, FILE *stream) 
{
  for(struct opt *opt = opts; opt->name; ++opt) {
    if(!opt->set)
      continue;

    fprintf(stream, "%s: ", opt->name);
    if(!(opt->flags & OPT_NOPARAM)) {
      arr_for(void *, param, &opt->params) {
        if(opt_is_int(opt))
          fprintf(stream, "%d", *(int*)param);
        else if(opt_is_float(opt))
          fprintf(stream, "%f", *(float*)param);
        else
          fprintf(stream, "%s", *(const char**)param);

        fprintf(stream, "%s", param != arr_lst(&opt->params) ? ", " : "");
      }
    }
    fprintf(stream, "\n");
  }
}

#endif /* _KK_OPTS_IMPL_ */

#endif /* KK_OPTS_IMPL */
