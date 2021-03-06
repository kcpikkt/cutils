#ifndef _KK_ARR_H_
#define _KK_ARR_H_

#include <stdlib.h>
#include <stdbool.h>

#if !defined(NDEBUG)

# include <stdio.h>
# include <assert.h>

# define ARR_ASSERT(COND) assert(COND)
# define ARR_ASSERT_MSG(COND, ...)\
  do {\
    if(!(COND)) {\
      fprintf(stderr, "%s:%s:%d: ", __FILE__, __func__, __LINE__);\
      fprintf(stderr, __VA_ARGS__);\
      fprintf(stderr, "\n");\
      fflush(stderr);\
      assert(COND);\
    }\
  } while(0)

#else 

# define ARR_ASSERT()
# define ARR_ASSERT_MSG()

#endif


#define ARR_LIKELY(X) __builtin_expect((X),1)

#define arr_for(TYPE, VAR, ARR) \
  for(TYPE * VAR = arr_fst(ARR);\
      (VAR);\
      VAR = arr_next(ARR, VAR))

#define arr_rfor(TYPE, VAR, ARR) \
  for(TYPE * VAR = arr_lst(ARR);\
      (VAR);\
      VAR = arr_prev((ARR), VAR))

struct arr {
  union { size_t _cap; const size_t cap; };
  union { size_t _cnt; const size_t cnt; };
  union { size_t _esz; const size_t esz; };
  union { void * _mem; const void * const mem; };
};

int arr_realloc(struct arr * arr, size_t ncap);

int arr_grow(struct arr * arr);

void arr_zero(struct arr *arr);

/* internal indexing function */
static inline
void * _arr_at(const struct arr * arr, size_t idx) {
  /* allow to index 'end' i.e. one past the last element */
  ARR_ASSERT_MSG(idx <= arr->cap, 
      "Index out of bounds, idx=%zu, arr->cap=%zu", idx, arr->cap);
  return (void*)((uint8_t*)arr->_mem + idx * arr->esz);
}

/* standard indexing
 * returns NULL out of bounds */
static inline
void * arr_at(const struct arr * arr, size_t idx)
{
  ARR_ASSERT(arr);
  return ( idx < arr->cnt ) ? _arr_at(arr, idx) : NULL;
}

/* signed indexing  
 * negative idx returns (cap + idx)'th element */ 
static inline
void * arr_sat(const struct arr * arr, ssize_t idx)
{
  if(ARR_LIKELY(idx < 0)) 
    return -idx <= arr->cnt ? _arr_at(arr, arr->cnt + idx) : NULL;

  return arr_at(arr, idx);
}

/* imperative indexing 
 * will crash rather than return NULL out of bounds */
static inline
void * arr_imp_at(const struct arr * arr, size_t idx)
{
  ARR_ASSERT(arr);
  ARR_ASSERT_MSG(idx < arr->cnt,
      "Index out of bounds, idx=%zu, arr->cnt=%zu", idx, arr->cap);
  return _arr_at(arr, idx);
}

/* imperative signed indexing 
 * will crash rather than return NULL out of bounds 
 * */
static inline
void * arr_imp_sat(const struct arr * arr, size_t idx)
{
  ARR_ASSERT(arr);
  ARR_ASSERT(idx < arr->cnt);
  return _arr_at(arr, idx);
}

void * arr_lst(const struct arr * arr);

void * arr_fst(const struct arr * arr);

void * arr_push(struct arr * arr, const void * e);

void arr_pop(struct arr * arr);

bool arr_belongs(const struct arr *arr, const void *e);

size_t arr_idx(const struct arr *arr, const void *e);

void * arr_prev(const struct arr *arr, const void *e);

void * arr_next(const struct arr * arr, const void *e);

void arr_append(struct arr * dst, struct arr * src);

void arr_prepend(struct arr * dst, struct arr * src);

void arr_qsort(struct arr * arr, int (*cmp)(const void *, const void *) );

int arr_init(struct arr * arr, size_t esz);

struct arr arr_new(size_t esz);

void arr_swap(struct arr *arr, void *a, void *b);

void arr_clear(struct arr *arr);

int arr_cleanup(struct arr * arr);

void arr_assign(struct arr * dst, struct arr * src);

void arr_assign_val(struct arr * dst, struct arr val);

int arr_remove_at(struct arr * arr, size_t idx);

int arr_remove(struct arr *arr, void *e);

int arr_insert_at(struct arr *arr, void *e, size_t idx);

void arr_uniq(struct arr * arr, int (*cmp)(const void *, const void *b));

void * arr_find(const struct arr * arr, const void *key, int (*cmp)(const void *, const void *));

int arr_copy(struct arr *dst, struct arr *src);

int arr_move(struct arr *dst, struct arr *src);

int arr_resize(struct arr *arr, size_t ncnt);

int arr_remove_in_loop(struct arr *arr, void *e, void ** const loop_e);

void arr_append_in_loop(struct arr * dst, struct arr * src, void ** const loop_e);

void arr_prepend_in_loop(struct arr * dst, struct arr * src, void ** const loop_e);

void * arr_push_in_loop(struct arr * arr, const void * e, void ** const loop_e);

void arr_print(struct arr *arr, const char * name);

#endif /* _KK_ARR_H_ */

/* implementation */

#ifdef KK_ARR_IMPL

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

//#define ARR_DBG

#ifdef ARR_DBG
#define ARR_DBG_PROC_HDR \
  puts(__func__);\
  fflush(stdout);
#else
#define ARR_DBG_PROC_HDR
#endif


#define _pdiff(A,B) (((uint8_t*)A) - ((uint8_t*)B))

static void _arr_expand(struct arr * arr, size_t idx, size_t n) {
  ARR_DBG_PROC_HDR;

  if(n == 0) return;

  uint8_t * from = _arr_at(arr, idx);
  uint8_t * to = from + arr->esz * n;

  assert(arr->cnt >= idx);
  size_t size = arr->esz * (arr->cnt - idx);

  memmove(to, from, size);
  memset(from, 0, (to - from));
}

int arr_realloc(struct arr * arr, size_t ncap) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);

  ARR_ASSERT(arr->cnt <= ncap);

  void * nmem = realloc(arr->_mem, ncap * arr->esz);

  //ARR_ARR_ASSERT((ncap == 0) == (nmem == 0));
  arr->_cap = ncap;
  arr->_mem = nmem;
  return 0;
}

int arr_grow(struct arr * arr) {
  ARR_DBG_PROC_HDR;
  return arr_realloc(arr, arr->cap ? arr->cap * 2 : 1);
}

void arr_zero(struct arr *arr) {
  memset(arr->_mem, '\0', arr->cap * arr->esz);
}

void * arr_lst(const struct arr * arr) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);
  return ( arr->cnt ) ? arr_at(arr, arr->cnt - 1) : NULL;
}

void * arr_fst(const struct arr * arr) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);
  return arr_at(arr, 0);
}


void * arr_push(struct arr * arr, const void * e) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);

  if(arr->cnt == arr->cap) 
    ARR_ASSERT( !arr_grow(arr) );

  ARR_ASSERT(arr->cnt < arr->cap) ;

  if(e) 
    memcpy( _arr_at(arr, arr->cnt), e, arr->esz );
  else
    memset( _arr_at(arr, arr->cnt), 0, arr->esz );

  return _arr_at(arr, arr->_cnt++);
}


void arr_pop(struct arr * arr) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr && arr->cnt);
  arr->_cnt--;
}

bool arr_belongs(const struct arr *arr, const void *e) {
  if((void*)arr->mem <= e && e <= (void*)arr_lst(arr)) {}
  else return false;

  size_t d =  _pdiff(e, arr->mem);

  if(d % arr->esz == 0) {}
  else return false;

  return true;
}

size_t _arr_idx(const struct arr *arr, const void *e) {
  return ( _pdiff(e, arr->mem) / arr->esz);
}

size_t arr_idx(const struct arr *arr, const void *e) {
  void * lst = arr_lst(arr);
  ARR_DBG_PROC_HDR;

  ARR_ASSERT( arr_belongs(arr, e) );
  return _arr_idx(arr, e);
}

void * arr_prev(const struct arr *arr, const void *e) {
  ARR_DBG_PROC_HDR;
  if(arr->cnt == 0) return NULL;

  void *lst = arr_lst(arr);
  void *end = (uint8_t*)lst + arr->esz;

  if(e == end) return lst;

  size_t idx = arr_idx(arr, e);
  return idx ? arr_at(arr, idx - 1) : NULL;
}

void * arr_next(const struct arr * arr, const void *e) {
  ARR_DBG_PROC_HDR;
  return arr_at(arr, _arr_idx(arr, e) + 1);
  //if(arr->cnt == 0) return NULL;

  //void *lst = arr_lst(arr);
  //void *end = (uint8_t*)lst + arr->esz;
  //if(e == end) return NULL;

  //size_t idx = arr_idx(arr, e);
  //return idx < arr->cnt ? arr_at(arr, idx + 1) : NULL;
}

// FIXME
void arr_append(struct arr * dst, struct arr * src) {
  ARR_DBG_PROC_HDR;
  assert(dst->esz == src->esz);

  arr_for(void, e, src) {
    arr_push(dst, e);
  }
}

void arr_prepend(struct arr * dst, struct arr * src) {
  ARR_DBG_PROC_HDR;
  assert(dst->esz == src->esz);

  if(src->cnt == 0) return;

  arr_realloc(dst, 100);

  _arr_expand(dst, 0, src->cnt);
  dst->_cnt += src->cnt;

  assert(dst->_mem && src->_mem);

  memcpy(dst->_mem, src->_mem, src->cnt * src->esz);
}


void arr_qsort(struct arr * arr, int (*cmp)(const void *, const void *) ) {
  ARR_DBG_PROC_HDR;
  qsort(arr->_mem, arr->cnt, arr->esz, cmp);
}

int arr_init(struct arr * arr, size_t esz) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr && esz);
  memset(arr, 0, sizeof(struct arr));

  arr->_esz = esz;
  arr->_mem = NULL;
  return 0;
}

struct arr arr_new(size_t esz) {
  struct arr arr;
  ARR_ASSERT( arr_init(&arr, esz) == 0 );
  return arr;
}

void arr_swap(struct arr *arr, void *a, void *b) {
  ARR_ASSERT(arr->mem <= a && a <= arr_lst(arr));
  ARR_ASSERT(arr->mem <= b && b <= arr_lst(arr));

  uint8_t tmp[arr->esz];

  memcpy(tmp, a, arr->esz);
  memcpy(b,   a, arr->esz);
  memcpy(b, tmp, arr->esz);
}

void arr_clear(struct arr *arr) {
  ARR_DBG_PROC_HDR;
  arr->_cnt = 0;
  ARR_ASSERT( !arr_realloc(arr, 0) );
}

int arr_cleanup(struct arr * arr) {
  ARR_DBG_PROC_HDR;
  
  arr->_cnt = 0;
  return arr_realloc(arr, 0);
}

void arr_assign(struct arr * dst, struct arr * src) {
  ARR_DBG_PROC_HDR;
  //FIXME
  memcpy(dst, src, sizeof(struct arr));
}

void arr_assign_val(struct arr * dst, struct arr val) {
  arr_assign(dst, &val);
}

static void _arr_collapse(struct arr * arr, size_t idx, size_t n) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr->cnt >= idx + n);

  if(n == 0) return;

  uint8_t * to = _arr_at(arr, idx);
  uint8_t * from = to + arr->esz * n;

  size_t size = arr->esz * (arr->cnt - idx - n);

  memmove(to, from, size);
}

int arr_remove_at(struct arr * arr, size_t idx) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr->cnt);

  _arr_collapse(arr, idx, 1);
  arr->_cnt--;

  return 0;
}

int arr_remove(struct arr *arr, void *e) {
  ARR_DBG_PROC_HDR;
  return arr_remove_at(arr, arr_idx(arr, e));
}

int arr_insert_at(struct arr *arr, void *e, size_t idx) {
  if(arr->cnt == arr->cap) 
    ARR_ASSERT( !arr_grow(arr) );

  _arr_expand(arr, idx, 1);
  arr->_cnt++;

  if(e) 
    memcpy( _arr_at(arr, idx), e, arr->esz );
  else
    memset( _arr_at(arr, idx), 0, arr->esz );

  return 0;
}


void arr_uniq(struct arr * arr, int (*cmp)(const void *, const void *b)) {
  ARR_DBG_PROC_HDR;
  void *e, *lkp;

  for(e = arr_lst(arr); e; e = arr_prev(arr, e)) {
    for(lkp = arr_prev(arr, e); lkp; lkp = arr_prev(arr, lkp)) {
      if( cmp(e, lkp) == 0 ) { arr_remove(arr, e); break; }
    }
  }
}

void * arr_find(const struct arr * arr, const void *key, int (*cmp)(const void *, const void *)) {
  arr_for(void, e, arr) {
    if( !cmp(key, e) ) return e;
  }
  return NULL;
}

int arr_copy(struct arr *dst, struct arr *src) 
{
  ARR_ASSERT(dst && src);
  ARR_ASSERT(src->esz)
  int rc = -1;

  if(dst->esz) 
    arr_cleanup(dst);

  if(arr_init(dst, src->esz)) 
    goto exit;

  if(arr_realloc(dst, src->cap)) 
    goto exit;

  memcpy(dst->_mem, src->_mem, src->esz * src->cap);
  dst->_cnt = src->cnt;
  dst->_cap = src->cap;
 exit:
  return rc;
}

static void _arr_zero_range(struct arr *arr, size_t b, size_t e) {
  ARR_ASSERT( b <= e );
  memset((uint8_t*)arr->_mem + b * arr->esz, '\0', e - b);
}

int arr_resize(struct arr *arr, size_t ncnt) {
  int rc;
  if(arr->cnt < ncnt)  {
    if((rc = arr_realloc(arr, ncnt)))
      goto exit;

    _arr_zero_range(arr, arr->cnt, ncnt);
  }
  arr->_cnt = ncnt;

 exit:
  return rc;
}


void * arr_push_in_loop(struct arr * arr, const void * e, void ** const loop_e) {
  size_t idx = arr_idx(arr, *loop_e);
  void *ret = arr_push(arr, e);
  *loop_e = arr_at(arr, idx);

  return ret;
}

int arr_remove_in_loop(struct arr * arr, void * e, void ** const loop_e) {
  size_t idx = arr_idx(arr, *loop_e);
  int ret = arr_remove(arr, e);
  *loop_e = arr_at(arr, idx);

  return ret;
}

void arr_append_in_loop(struct arr * dst, struct arr * src, void ** const loop_e) {
  size_t idx = arr_idx(dst, *loop_e);
  arr_append(dst, src);
  *loop_e = arr_at(dst, idx);
}

void arr_prepend_in_loop(struct arr * dst, struct arr * src, void ** const loop_e) {
  size_t idx = arr_idx(dst, *loop_e);
  size_t before = dst->cnt;
  arr_prepend(dst, src);
  assert(before + src->cnt == dst->cnt);
  *loop_e = arr_at(dst, idx + src->cnt);
}


void arr_print(struct arr *arr, const char * name) {
  if(name) printf("%s ", name);
  printf("{ cap: %zu, cnt: %zu, esz: %zu, mem: %p }" ,
        arr->cap, arr->cnt, arr->esz, arr->mem);
}

#endif /* #ifdef KK_ARR_IMPL */