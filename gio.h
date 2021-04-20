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
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

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
  ssize_t (*write)(void *, const void *, size_t);
  ssize_t (*read)(void *, void *, size_t);
  off_t (*seek)(void *, off_t, int);
  int (*sync)(void *);
  int (*close)(void *);
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

/* generic api */

KK_GIO_API
ssize_t gio_write(struct gio *_gio, const void *buf, size_t sz);

KK_GIO_API
ssize_t gio_read(struct gio *_gio, void *buf, size_t sz);

KK_GIO_API
off_t gio_seek(struct gio *_gio, off_t off, int whence);

KK_GIO_API
int gio_printf(struct gio *_gio, const char *fmt, ...);

KK_GIO_API
int gio_sync(struct gio *_gio);

KK_GIO_API
int gio_close(struct gio *_gio);

#endif /* _KK_GIO_H_ */

/* implementation */

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

  if(fcntl(fd, F_GETFD) != -1 || errno != EBADF)
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
ssize_t gio_write(struct gio *_gio, const void *buf, size_t sz)
{
  switch(_gio->type) {
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
      return gio->write(gio->data, buf, sz);
    else
      return -1;
  }
  defualt:
    assert(0);
  }
  return -1;
}

KK_GIO_API
ssize_t gio_read(struct gio *_gio, void *buf, size_t sz)
{
  switch(_gio->type) {
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
      return gio->read(gio->data, buf, sz);
    else
      return -1;
  }
  defualt:
    assert(0);
  }
  return -1;
}

KK_GIO_API
off_t gio_seek(struct gio *_gio, off_t off, int whence)
{
  switch(_gio->type) {
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
      return gio->seek(gio->data, off, whence);
    else
      return -1;
  }
  defualt:
    assert(0);
  }
  return -1;
}

KK_GIO_API
int gio_printf(struct gio *_gio, const char *fmt, ...) 
{
  int ret = -1;
  va_list args;
  va_start(args, fmt);

  switch(_gio->type) { 
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
      ret = vdprintf(gio->fd, fmt, args);
    break;
  }
  case GIO_OPS: {
    break;
  }
  defualt:
    assert(0);
  }

  va_end(args);
  return ret;
}

KK_GIO_API
int gio_sync(struct gio *_gio)
{  
  int ret = -1;

  switch(_gio->type) {
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
      return gio->sync(gio->data);
    else
      return 0; /* no sync, no problem */
  }
  defualt:
    assert(0);
  }
}

KK_GIO_API
int gio_close(struct gio *_gio) 
{
  switch(_gio->type) {
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
      return gio->close(gio->data);
    else
      return 0; /* no close, no problem */
  }
  defualt:
    assert(0);
  }
  return -1;
}

#endif /* KK_GIO_IMPL */
