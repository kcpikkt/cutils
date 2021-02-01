#include "arr.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "debug.h"

#define ARR_ASSERT(...) \
  {\
    if( !(__VA_ARGS__) ) {\
      print_trace();\
      assert(__VA_ARGS__);\
    }\
  }


//#define ARR_DBG

#ifdef ARR_DBG
#define ARR_DBG_PROC_HDR \
  puts(__func__);\
  fflush(stdout);
#else
#define ARR_DBG_PROC_HDR
#endif


#define _pdiff(A,B) (((uint8_t*)A) - ((uint8_t*)B))


static void * _arr_at(const Arr * arr, size_t idx) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(idx <= arr->cap); // we can index end
  return (void*)((uint8_t*)arr->_mem + idx * arr->esz);
}


static void _arr_expand(Arr * arr, size_t idx, size_t n) {
  ARR_DBG_PROC_HDR;

  if(n == 0) return;

  uint8_t * from = _arr_at(arr, idx);
  uint8_t * to = from + arr->esz * n;

  assert(arr->cnt >= idx);
  size_t size = arr->esz * (arr->cnt - idx);

  memmove(to, from, size);
  memset(from, 0, (to - from));
}

int arr_realloc(Arr * arr, size_t ncap) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);

  ARR_ASSERT(arr->cnt <= ncap);

  void * nmem = realloc(arr->_mem, ncap * arr->esz);

  //ARR_ARR_ASSERT((ncap == 0) == (nmem == 0));
  arr->_cap = ncap;
  arr->_mem = nmem;
  return 0;
}

int arr_grow(Arr * arr) {
  ARR_DBG_PROC_HDR;
  return arr_realloc(arr, arr->cap ? arr->cap * 2 : 1);
}

void arr_zero(Arr *arr) {
  memset(arr->_mem, '\0', arr->cap * arr->esz);
}
void * arr_at(const Arr * arr, size_t idx) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);
  return ( idx < arr->cnt ) ? _arr_at(arr, idx) : NULL;
}

void * arr_lst(const Arr * arr) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);
  return ( arr->cnt ) ? arr_at(arr, arr->cnt - 1) : NULL;
}

void * arr_fst(const Arr * arr) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr);
  return arr_at(arr, 0);
}


void * arr_push(Arr * arr, const void * e) {
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


void arr_pop(Arr * arr) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr && arr->cnt);
  arr->_cnt--;
}

bool arr_belongs(const Arr *arr, const void *e) {
  if((void*)arr->mem <= e && e <= (void*)arr_lst(arr)) {}
  else return false;

  size_t d =  _pdiff(e, arr->mem);

  if(d % arr->esz == 0) {}
  else return false;

  return true;
}

size_t _arr_idx(const Arr *arr, const void *e) {
  return ( _pdiff(e, arr->mem) / arr->esz);
}

size_t arr_idx(const Arr *arr, const void *e) {
  void * lst = arr_lst(arr);
  ARR_DBG_PROC_HDR;

  ARR_ASSERT( arr_belongs(arr, e) );
  return _arr_idx(arr, e);
}

void * arr_prev(const Arr *arr, const void *e) {
  ARR_DBG_PROC_HDR;
  if(arr->cnt == 0) return NULL;

  void *lst = arr_lst(arr);
  void *end = (uint8_t*)lst + arr->esz;

  if(e == end) return lst;

  size_t idx = arr_idx(arr, e);
  return idx ? arr_at(arr, idx - 1) : NULL;
}

void * arr_next(const Arr * arr, const void *e) {
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
void arr_append(Arr * dst, Arr * src) {
  ARR_DBG_PROC_HDR;
  assert(dst->esz == src->esz);

  arr_for(void, e, src) {
    arr_push(dst, e);
  }
}

void arr_prepend(Arr * dst, Arr * src) {
  ARR_DBG_PROC_HDR;
  assert(dst->esz == src->esz);

  if(src->cnt == 0) return;

  arr_realloc(dst, 100);

  _arr_expand(dst, 0, src->cnt);
  dst->_cnt += src->cnt;

  assert(dst->_mem && src->_mem);

  memcpy(dst->_mem, src->_mem, src->cnt * src->esz);
}


void arr_qsort(Arr * arr, int (*cmp)(const void *, const void *) ) {
  ARR_DBG_PROC_HDR;
  qsort(arr->_mem, arr->cnt, arr->esz, cmp);
}

int arr_init(Arr * arr, size_t esz) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr && esz);
  memset(arr, 0, sizeof(Arr));

  arr->_esz = esz;
  arr->_mem = NULL;
  return 0;
}

Arr arr_new(size_t esz) {
  Arr arr;
  ARR_ASSERT( arr_init(&arr, esz) == 0 );
  return arr;
}

void arr_swap(Arr *arr, void *a, void *b) {
  ARR_ASSERT(arr->mem <= a && a <= arr_lst(arr));
  ARR_ASSERT(arr->mem <= b && b <= arr_lst(arr));

  uint8_t tmp[arr->esz];

  memcpy(tmp, a, arr->esz);
  memcpy(b,   a, arr->esz);
  memcpy(b, tmp, arr->esz);
}

void arr_clear(Arr *arr) {
  ARR_DBG_PROC_HDR;
  arr->_cnt = 0;
  ARR_ASSERT( !arr_realloc(arr, 0) );
}

int arr_cleanup(Arr * arr) {
  ARR_DBG_PROC_HDR;
  
  arr->_cnt = 0;
  return arr_realloc(arr, 0);
}

void arr_assign(Arr * dst, Arr * src) {
  ARR_DBG_PROC_HDR;
  //FIXME
  memcpy(dst, src, sizeof(Arr));
}

void arr_assign_val(Arr * dst, Arr val) {
  arr_assign(dst, &val);
}

static void _arr_collapse(Arr * arr, size_t idx, size_t n) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr->cnt >= idx + n);

  if(n == 0) return;

  uint8_t * to = _arr_at(arr, idx);
  uint8_t * from = to + arr->esz * n;

  size_t size = arr->esz * (arr->cnt - idx - n);

  memmove(to, from, size);
}

int arr_remove_at(Arr * arr, size_t idx) {
  ARR_DBG_PROC_HDR;
  ARR_ASSERT(arr->cnt);

  _arr_collapse(arr, idx, 1);
  arr->_cnt--;

  return 0;
}

int arr_remove(Arr *arr, void *e) {
  ARR_DBG_PROC_HDR;
  return arr_remove_at(arr, arr_idx(arr, e));
}

int arr_insert_at(Arr *arr, void *e, size_t idx) {
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


void arr_uniq(Arr * arr, int (*cmp)(const void *, const void *b)) {
  ARR_DBG_PROC_HDR;
  void *e, *lkp;

  for(e = arr_lst(arr); e; e = arr_prev(arr, e)) {
    for(lkp = arr_prev(arr, e); lkp; lkp = arr_prev(arr, lkp)) {
      if( cmp(e, lkp) == 0 ) { arr_remove(arr, e); break; }
    }
  }
}

void * arr_find(const Arr * arr, const void *key, int (*cmp)(const void *, const void *)) {
  arr_for(void, e, arr) {
    if( !cmp(key, e) ) return e;
  }
  return NULL;
}

int arr_copy(Arr *dst, Arr *src) {
  ARR_DBG_PROC_HDR;
  assert(dst && src);
  int rc;
  if(( rc = arr_init(dst, src->esz)    )) goto exit;
  if(( rc = arr_realloc(dst, src->cap) )) goto exit;
  memcpy(dst->_mem, src->_mem, src->esz * src->cap);
  dst->_cnt = src->cnt;
  dst->_cap = src->cap;
 exit:
  return rc;
}

static void _arr_zero_range(Arr *arr, size_t b, size_t e) {
  ARR_ASSERT( b <= e );
  memset((uint8_t*)arr->_mem + b * arr->esz, '\0', e - b);
}

int arr_resize(Arr *arr, size_t ncnt) {
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



void * arr_push_in_loop(Arr * arr, const void * e, void ** const loop_e) {
  size_t idx = arr_idx(arr, *loop_e);
  void *ret = arr_push(arr, e);
  *loop_e = arr_at(arr, idx);

  return ret;
}

int arr_remove_in_loop(Arr * arr, void * e, void ** const loop_e) {
  size_t idx = arr_idx(arr, *loop_e);
  int ret = arr_remove(arr, e);
  *loop_e = arr_at(arr, idx);

  return ret;
}

void arr_append_in_loop(Arr * dst, Arr * src, void ** const loop_e) {
  size_t idx = arr_idx(dst, *loop_e);
  arr_append(dst, src);
  *loop_e = arr_at(dst, idx);
}

void arr_prepend_in_loop(Arr * dst, Arr * src, void ** const loop_e) {
  size_t idx = arr_idx(dst, *loop_e);
  size_t before = dst->cnt;
  arr_prepend(dst, src);
  assert(before + src->cnt == dst->cnt);
  *loop_e = arr_at(dst, idx + src->cnt);
}




void arr_print(Arr *arr, const char * name) {
  if(name) printf("%s ", name);
  printf("{ cap: %zu, cnt: %zu, esz: %zu, mem: %p }" ,
        arr->cap, arr->cnt, arr->esz, arr->mem);
}
