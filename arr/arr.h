#ifndef _ANN_ARR_H_
#define _ANN_ARR_H_

#include <stdlib.h>
#include <stdbool.h>

#define arr_for(TYPE, VAR, ARR) \
  for(TYPE * VAR = arr_fst(ARR); VAR; VAR = arr_next(ARR, VAR))

//FIXME
#define arr_for_extvar(TYPE, VAR, ARR) \
  TYPE * VAR = arr_fst(ARR); for(; VAR; VAR = arr_next((ARR), VAR))

#define arr_rfor(TYPE, VAR, ARR) \
  for(TYPE * VAR = arr_lst(ARR); VAR; VAR = arr_prev((ARR), VAR))

struct arr {
  union { size_t _cap; const size_t cap; };
  union { size_t _cnt; const size_t cnt; };
  union { size_t _esz; const size_t esz; };
  union { void * _mem; const void * const mem; };
};
typedef struct arr Arr;

int arr_realloc(Arr * arr, size_t ncap);

int arr_grow(Arr * arr);

void arr_zero(Arr *arr);

void * arr_at(const Arr * arr, size_t idx);

void * arr_at(const Arr * arr, size_t idx);

// add imperative - bounds checking, does not return NULL
void * arr_imp_at(const Arr * arr, size_t idx);

void * arr_lst(const Arr * arr);

void * arr_fst(const Arr * arr);

void * arr_push(Arr * arr, const void * e);

void arr_pop(Arr * arr);

bool arr_belongs(const Arr *arr, const void *e);

size_t arr_idx(const Arr *arr, const void *e);

void * arr_prev(const Arr *arr, const void *e);

void * arr_next(const Arr * arr, const void *e);

void arr_append(Arr * dst, Arr * src);

void arr_prepend(Arr * dst, Arr * src);

void arr_qsort(Arr * arr, int (*cmp)(const void *, const void *) );

int arr_init(Arr * arr, size_t esz);

Arr arr_new(size_t esz);

void arr_swap(Arr *arr, void *a, void *b);

void arr_clear(Arr *arr);

int arr_cleanup(Arr * arr);

void arr_assign(Arr * dst, Arr * src);

void arr_assign_val(Arr * dst, Arr val);

int arr_remove_at(Arr * arr, size_t idx);

int arr_remove(Arr *arr, void *e);

int arr_insert_at(Arr *arr, void *e, size_t idx);

void arr_uniq(Arr * arr, int (*cmp)(const void *, const void *b));

void * arr_find(const Arr * arr, const void *key, int (*cmp)(const void *, const void *));

int arr_copy(Arr *dst, Arr *src);

int arr_resize(Arr *arr, size_t ncnt);

int arr_remove_in_loop(Arr *arr, void *e, void ** const loop_e);

void arr_append_in_loop(Arr * dst, Arr * src, void ** const loop_e);

void arr_prepend_in_loop(Arr * dst, Arr * src, void ** const loop_e);

void * arr_push_in_loop(Arr * arr, const void * e, void ** const loop_e);

void arr_print(Arr *arr, const char * name);

#endif /* _ANN_ARR_H_ */
