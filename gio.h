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
 *  Single header generic input/output interface.
 *  Helpful for implementing something like IO buffering or 
 *  single input multiple output and more.
 */

#ifndef _KK_GIO_H_
#define _KK_GIO_H_
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef KK_GIO_XALLOC
# include <stdlib.h>
static inline void * _kk_gio_xalloc(void *ptr, size_t sz) 
{
  return (!sz ? (free(ptr), NULL) : (!ptr ? malloc(sz) : realloc(ptr, sz)));
}
# define KK_GIO_XALLOC(PTR, SZ) _kk_gio_xalloc(PTR, SZ)
#endif

#ifndef KK_GIO_ASSERT
# include <assert.h>
# define KK_GIO_ASSERT(...) assert(__VA_ARGS__)
#endif

#define KK_GIO_UNREACHABLE() KK_GIO_ASSERT(0 && "Unreachable code reached.")

/* In some cases we buffer printf ourselves. Then this is space of initial 
 * buffer, which may be e.g. on stack to avoid allocation. Only if that buffer
 * turns out to be too small, a buffer with an appropriate size is allocated.
 */
#ifndef KK_GIO_PRINTF_BUFSZ
# define KK_GIO_PRINTF_BUFSZ 256
#endif

#ifdef KK_GIO_ENABLE_TRACING
# ifndef KK_GIO_TRACE
#  define KK_GIO_TRACE(...) printf(__VA_ARGS__); fflush(stdout)
# endif
#else
# undef KK_GIO_TRACE
# define KK_GIO_TRACE(...)
#endif

enum {
  GIO_INVALID,
  GIO_FILE,
  GIO_MEM,
  GIO_OPS,
};

struct gio {
  uint8_t type;
  uint8_t flags;
};

struct gio_mem {
  struct gio gio;
  void *buf;
  size_t sz;
  size_t off;
};

struct gio_file {
  struct gio gio;
  FILE *fp;
  int fd;
};

struct gio_ops {
  struct gio gio;
  void *data;
  ssize_t (*write)(struct gio_ops *, const void *, size_t);
  ssize_t (*read)(struct gio_ops *, void *, size_t);
  off_t (*seek)(struct gio_ops *, off_t, int);
  int (*sync)(struct gio_ops *);
  int (*close)(struct gio_ops *);
};

#define KK_GIO_API

static inline
bool gio_alive(struct gio *gio)
{
  return gio->type;
}

#define GIO_MAX(A,B) ((A) < (B) ? (B) : (A))
#define GIO_MIN(A,B) ((A) > (B) ? (B) : (A))

/* in memory api */

KK_GIO_API
int gio_mem_init(struct gio_mem *gio, void *buf, size_t sz);

KK_GIO_API
struct gio_mem gio_mem_new(void *buf, size_t sz);

/* file pointer api */

KK_GIO_API
int gio_file_init_fp(struct gio_file *gio, FILE *fp);

KK_GIO_API
struct gio_file gio_file_new_fp(FILE *file);

/* file descriptor api */

KK_GIO_API
int gio_file_init_fd(struct gio_file *gio, int fd);

KK_GIO_API
struct gio_file gio_file_new_fd(int fd);

/* file api */

KK_GIO_API
int gio_file_init_open(const char *filepath);

KK_GIO_API
struct gio_file gio_file_new_open(const char *filepath);

/* list api */

enum {
  GIO_LIST_READ       = (1 << 0), /* not impelmented */
  GIO_LIST_FAIL_FAST  = (1 << 1), 
};

KK_GIO_API
struct gio_ops gio_list_new(uint8_t flags);

/* This function makes a copy of the struct pointed to by gio. 
 * The ownership of gio also is transferred to the list, 
 * so user must not use or call any functions with gio afterwards.
 */
KK_GIO_API
int gio_list_add(struct gio_ops *list, struct gio *gio);

//int gio_imux_add(struct gio *gio);

/* generic api */

KK_GIO_API
ssize_t gio_write(void *_gio, const void *buf, size_t sz);

KK_GIO_API
ssize_t gio_read(void *_gio, void *buf, size_t sz);

KK_GIO_API
off_t gio_seek(void *_gio, off_t off, int whence);

KK_GIO_API
int gio_printf(void *_gio, const char *fmt, ...);

KK_GIO_API
int gio_vprintf(void *_gio, const char *fmt, va_list args);

KK_GIO_API
int gio_sync(void *_gio);

KK_GIO_API
int gio_close(void *_gio);

#endif /* _KK_GIO_H_ */

/* 
 *
 *
 * implementation 
 *
 *
 */

#ifdef KK_GIO_IMPL

/* in memory api */

KK_GIO_API
int gio_mem_init(struct gio_mem *gio, void *buf, size_t sz)
{
  if(gio_alive(&gio->gio))
    return -1;

  memset(gio, 0, sizeof(*gio));

  gio->gio.type = GIO_MEM;
  gio->buf = buf;
  gio->sz = sz;
  gio->off = 0;
  return 0;
}

KK_GIO_API
struct gio_mem gio_mem_new(void *buf, size_t sz)
{
  struct gio_mem gio_mem = {};
  int rc = gio_mem_init(&gio_mem, buf, sz);
  assert(rc == 0);
  return gio_mem;
}

/* file pointer api */

KK_GIO_API
int gio_file_init_fp(struct gio_file *gio, FILE *fp)
{
  if(gio_alive(&gio->gio))
    return -1;

  if(fp == NULL)
    return -1;

  memset(gio, 0, sizeof(*gio));

  gio->gio.type = GIO_FILE;
  gio->fp = fp;
  return 0;
}

KK_GIO_API
struct gio_file gio_file_new_fp(FILE *file)
{
  struct gio_file gio_file = {};
  int rc = gio_file_init_fp(&gio_file, file);
  assert(rc == 0);
  return gio_file;
}

/* file descriptor api */

KK_GIO_API
int gio_file_init_fd(struct gio_file *gio, int fd)
{
  if(gio_alive(&gio->gio))
    return -1;

  if(fcntl(fd, F_GETFD) == -1 || errno == EBADF)
    return -1;

  memset(gio, 0, sizeof(*gio));

  gio->gio.type = GIO_FILE;
  gio->fd = fd;
  return 0;
}

KK_GIO_API
struct gio_file gio_file_new_fd(int fd)
{
  struct gio_file gio_file = {};
  int rc = gio_file_init_fd(&gio_file, fd);
  assert(rc == 0);
  return gio_file;
}

/* generic api */

KK_GIO_API
ssize_t gio_write(void *_gio, const void *buf, size_t sz)
{
  KK_GIO_TRACE(
      "%s(): gio=%p { .type=%d }, buf=%p, sz=%zu\n", 
      __func__, _gio, ((struct gio*)_gio)->type, buf, sz);

  switch(((struct gio*)_gio)->type) {
  case GIO_MEM: {
    struct gio_mem *gio = (struct gio_mem*)_gio;
    assert(gio->sz >= gio->off);
    sz = GIO_MIN(sz, gio->sz - gio->off);
    memcpy(gio->buf + gio->off, buf, sz);
    gio->off += sz;
    return sz;
  }
  case GIO_FILE: {
    struct gio_file *gio = (struct gio_file*)_gio;
    if(gio->fp)
      return fwrite(buf, 1, sz, gio->fp);
    else
      return write(gio->fd, buf, sz);
  }
  case GIO_OPS: {
    struct gio_ops *gio = (struct gio_ops*)_gio;
    if(gio->write) 
      return gio->write(gio, buf, sz);
    else
      return -1;
  }
  defualt:
    KK_GIO_UNREACHABLE();
  }
  return -1;
}

KK_GIO_API
ssize_t gio_read(void *_gio, void *buf, size_t sz)
{
  KK_GIO_TRACE(
      "%s(): gio=%p { .type=%d }, buf=%p, sz=%zu\n", 
      __func__, _gio, ((struct gio*)_gio)->type, buf, sz);

  switch(((struct gio*)_gio)->type) {
  case GIO_MEM: {
    struct gio_mem *gio = (struct gio_mem*)_gio;
    assert(gio->sz >= gio->off);
    sz = GIO_MIN(sz, gio->sz - gio->off);
    memcpy(buf, gio->buf + gio->off, sz);
    gio->off += sz;
    return sz;
  }
  case GIO_FILE: {
    struct gio_file *gio = (struct gio_file*)_gio;
    if(gio->fp)
      return fread(buf, 1, sz, gio->fp);
    else
      return read(gio->fd, buf, sz);
  }
  case GIO_OPS: {
    struct gio_ops *gio = (struct gio_ops*)_gio;
    if(gio->read)
      return gio->read(gio, buf, sz);
    else
      return -1;
  }
  defualt:
    KK_GIO_UNREACHABLE();
  }
  return -1;
}

KK_GIO_API
off_t gio_seek(void *_gio, off_t off, int whence)
{
  KK_GIO_TRACE(
      "%s(): gio=%p { .type=%d }, off=%d, whence=%d\n", 
      __func__, _gio, ((struct gio*)_gio)->type, off, whence);

  switch(((struct gio*)_gio)->type) {
  case GIO_MEM: {
    struct gio_mem *gio = (struct gio_mem*)_gio;
    switch(whence) {
    case SEEK_SET:
      if(off < 0 || off > gio->sz)
        return -1;

      gio->off = off;
      break;

    case SEEK_CUR:
      if(off >= 0) {
        if(off > gio->sz - gio->off)
          return -1;

      } else {
        if(-off > gio->off)
          return -1;
      }
      gio->off += off;
      break;

    case SEEK_END:
      if(off > 0 || -off > gio->sz)
        return -1;

      gio->off = gio->sz + off;
      break;

    default:
      return -1;
    }
    return gio->off;
  }
  case GIO_FILE: {
    struct gio_file *gio = (struct gio_file*)_gio;
    if(gio->fp)
      return fseek(gio->fp, off, whence);
    else
      return lseek(gio->fd, off, whence);
  }
  case GIO_OPS: {
    struct gio_ops *gio = (struct gio_ops*)_gio;
    if(gio->seek)
      return gio->seek(gio, off, whence);
    else
      return -1;
  }
  defualt:
    KK_GIO_UNREACHABLE();
  }
  return -1;
}

static inline
void * _gio_alloc_snprintf(int *ret_sz, const char *fmt, va_list args) 
{
  int len = KK_GIO_PRINTF_BUFSZ;
  char *buf;

  if(!(buf = KK_GIO_XALLOC(NULL, KK_GIO_PRINTF_BUFSZ)))
    return NULL;

  if((len = vsnprintf(buf, KK_GIO_PRINTF_BUFSZ, fmt, args)) < 0)
    return NULL;

  if(len > KK_GIO_PRINTF_BUFSZ) {
    if(!(buf = KK_GIO_XALLOC(NULL, len)))
      return NULL;

    if((len = vsnprintf(buf, len, fmt, args)) < 0)
      return NULL;
  }
  if(ret_sz)
    *ret_sz = len;

  return buf;
}

KK_GIO_API
int gio_vprintf(void *_gio, const char *fmt, va_list args) 
{
  int ret = -1;

  switch(((struct gio*)_gio)->type) { 
  case GIO_MEM: {
    struct gio_mem *gio =  (struct gio_mem*)_gio;
    ret = vsnprintf(gio->buf + gio->off, gio->sz - gio->off, fmt, args);
    if(ret >= 0) 
      gio->off += (ret = GIO_MIN(ret, gio->sz - gio->off));
    break;
  }
  case GIO_FILE: {
    struct gio_file *gio = (struct gio_file*)_gio;

    if(gio->fp)
      ret = vfprintf(gio->fp, fmt, args);
    else
      // TODO: test _POSIX_SOURCE etc 
      ret = -1; // vdprintf(gio->fd, fmt, args);
    break;
  }
  case GIO_OPS: {
    int bufsz;
    char *buf = _gio_alloc_snprintf(&bufsz, fmt, args);
    if(buf)
      ret = gio_write(_gio, buf, bufsz);

    KK_GIO_XALLOC(buf, 0);
    break;
  }
  defualt:
    KK_GIO_UNREACHABLE();
  }
  return ret;
}

KK_GIO_API
int gio_printf(void *_gio, const char *fmt, ...) 
{
  int ret = -1;
  va_list args;
  va_start(args, fmt);
  ret = gio_vprintf(_gio, fmt, args);
  va_end(args);
  return ret;
}

KK_GIO_API
int gio_sync(void *_gio)
{  
  switch(((struct gio*)_gio)->type) {
  case GIO_MEM: {
    return 0;
  }
  case GIO_FILE: {
    struct gio_file *gio = (struct gio_file*)_gio;
    if(gio->fp)
      return fflush(gio->fp);
    else
      return fsync(gio->fd);
  }
  case GIO_OPS: {
    struct gio_ops *gio = (struct gio_ops*)_gio;
    if(gio->sync)
      return gio->sync(gio);
    else
      return 0; /* no sync, no problem */
  }
  defualt:
    KK_GIO_UNREACHABLE();
  }
  return -1;
}

KK_GIO_API
int gio_close(void *_gio) 
{
  switch(((struct gio*)_gio)->type) {
  case GIO_MEM: {
    return 0;
  }
  case GIO_FILE: {
    struct gio_file *gio = (struct gio_file*)_gio;
    if(gio->fp)
      return fclose(gio->fp);
    else
      return close(gio->fd);
  }
  case GIO_OPS: {
    struct gio_ops *gio = (struct gio_ops*)_gio;
    if(gio->close)
      return gio->close(gio);
    else
      return 0; /* no close, no problem */
  }
  defualt:
    KK_GIO_UNREACHABLE();
  }
  return -1;
}

struct gio_list_data {
  uint8_t flags;
  size_t list_size;
  uint8_t list[];
};

static inline
ssize_t _gio_size(const struct gio *gio)
{
  switch(gio->type) {
  case GIO_MEM: 
    return sizeof(struct gio_mem);
  case GIO_FILE: 
    return sizeof(struct gio_file);
  case GIO_OPS: 
    return sizeof(struct gio_ops);
  }
  return -1;
}


struct gio *_gio_list_next(struct gio_list_data *data, struct gio *gio) 
{
  void *list_end;
  size_t gio_sz;

  if(!data || data->list_size == 0)
    return NULL;

  if(!gio)
    return (struct gio*)data->list;

  list_end = data->list + data->list_size;


  gio_sz = _gio_size(gio);
  KK_GIO_ASSERT(gio_sz > 0);
  gio = (struct gio*)((uint8_t*)gio + gio_sz);

  KK_GIO_ASSERT(
      (void*)data->list <= (void*)gio && (void*)gio <= (void*)list_end);

  if((void*)gio == list_end) 
    gio = NULL;

  return gio;
}

KK_GIO_API
int gio_list_add(struct gio_ops *list, struct gio *gio) 
{
  ssize_t gio_sz;
  void *new_data;
  struct gio_list_data *data = list->data;

  if(!list || !gio)
    return -1;

  gio_sz = _gio_size(gio);
  KK_GIO_ASSERT(gio_sz > 0);

  new_data = KK_GIO_XALLOC(list->data, 
        sizeof(struct gio_list_data) + data->list_size + gio_sz);

  if(new_data) {
    data = list->data = new_data;
    memcpy(data->list + data->list_size, gio, gio_sz);
    data->list_size += gio_sz;
    return 0;
  }
  return -1;
}


KK_GIO_API
ssize_t gio_list_write_cb(struct gio_ops *gio_ops, const void *buf, size_t sz)
{
  ssize_t ret, min = sz;
  struct gio *gio = NULL;

  while((gio = _gio_list_next(gio_ops->data, gio))) {
    ret = gio_write(gio, buf, sz);

    if(gio_ops->gio.flags & GIO_LIST_FAIL_FAST && ret < 0) 
      return ret;

    if(ret < min)
      min = ret;
  }
  return min;
#if 0
  struct gio *gio = NULL;
  struct gio_list_data *data = gio_ops->data;

  while(gio = _gio_list_next(data, gio)) {
    ret = gio_write(gio, buf, sz);

    if(data->flags & GIO_LIST_FAIL_FAST && ret < 0) 
      return ret;

    if(ret < min)
      min = ret;
  }

#endif
  return min;
}
#if 0

KK_GIO_API
ssize_t gio_list_read_cb(struct gio_ops *gio_ops, void *buf, size_t sz)
{
  ssize_t ret, min = sz;
  struct gio *gio = NULL;
  struct gio_list_data *data = gio_ops->data;

  while(gio = _gio_list_next(data, gio)) {
    ret = gio_read(gio, buf, sz);

    if(data->flags & GIO_LIST_FAIL_FAST && ret < 0) 
      return ret;

    if(ret < min)
      min = ret;
  }

  return min;
}


{
  ssize_t ret, min = 0;
  struct gio *gio = NULL;
  struct gio_list_data *data = _data

#if 0
  while(gio = _gio_list_next(data, gio)) {
    ret = gio_read(gio, buf, sz);

    if(ret != sz) {
      if(ret < min)
        min = ret;

      if(data->flags & GIO_LIST_FAIL_FAST)
        return ret;
    }
  }
#endif
  return min;
}

KK_GIO_API
off_t gio_list_seek_cb(void *_data, off_t off, int whence)
{
  if(!_data) return 0;
}

KK_GIO_API
int gio_list_sync_cb(void *_data)
{
  ssize_t ret, min = 0;
  struct gio *gio = NULL;
  struct gio_list_data *data = _data

#if 0
  while(gio = _gio_list_next(data, gio)) {
    ret = gio_close(gio, buf, sz);
  }
#endif
  return min;
}

KK_GIO_API
ssize_t gio_list_close_cb(void *_data, const void *buf, size_t sz)
{
  ssize_t ret, min = 0;
  struct gio *gio = NULL;
  struct gio_list_data *data = _data

#if 0
  while(gio = _gio_list_next(data, gio)) {
    ret = gio_close(gio, buf, sz);
  }
#endif
  KK_GIO_XALLOC(data, 0);
  return min;
}

#endif
KK_GIO_API
struct gio_ops gio_list_new(uint8_t flags)
{
  struct gio_ops ops = {
    .gio = {
      .type = GIO_OPS,
      .flags = flags,
    },
    .write = !(flags & GIO_LIST_READ) ? gio_list_write_cb : NULL,
    //.read = (flags & GIO_LIST_READ) ? gio_list_read_cb : NULL,
//    .seek = gio_list_seek_cb,
//    .sync = gio_list_sync_cb,
//    .close = gio_list_close_cb,
    .data = NULL,
  };

#if 0
  ops.data = KK_GIO_XALLOC(NULL, sizeof(struct gio_list_data));
  if(!ops.data)
    KK_GIO_UNREACHABLE();

  memset(ops.data, 0, sizeof(struct gio_list_data));

  ((struct gio_list_data*)ops.data)->flags = flags;
#endif
  return ops;
}

#endif /* KK_GIO_IMPL */
