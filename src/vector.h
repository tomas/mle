#ifndef VECTOR_H
#define VECTOR_H

#include <stdio.h>

// our very own vector implementation. from:
// http://eddmann.com/posts/implementing-a-dynamic-vector-array-in-c/

typedef struct vector {
  void **items;
  int capacity;
  int total;
} vector;

void vector_init(vector *v, int capacity) {
  v->capacity = capacity;
  v->total = 0;
  v->items = malloc(1024);
}

static void vector_resize(vector *v, int capacity) {
  void **items = realloc(v->items, sizeof(void *) * capacity);
  if (items) {
    v->items = items;
    v->capacity = capacity;
  }
}

int vector_add(vector *v, void *item) {
  if (v->capacity == v->total)
    vector_resize(v, v->capacity * 2);

  v->items[v->total++] = item;

  // printf(" -- added. total is %d\n", v->total);
  return v->total;
}

void *vector_get(vector *v, int index) {
  if (index >= 0 && index < v->total)
    return v->items[index];

  // printf(" -- null. total is %d\n", v->total);
  return NULL;
}

void vector_delete(vector *v, int index) {
  if (index < 0 || index >= v->total)
      return;

  v->items[index] = NULL;

  int i;
  for (i = 0; i < v->total - 1; i++) {
      v->items[i] = v->items[i + 1];
      v->items[i + 1] = NULL;
  }

  v->total--;

  if (v->total > 0 && v->total == v->capacity / 4)
      vector_resize(v, v->capacity / 2);
}

int vector_size(vector *v) {
  return v->total;
}

void vector_free(vector *v) {
  free(v->items);
}

#endif
