#include <stdio.h>
#include <malloc.h>

static void*(*org_malloc_hook)(size_t, const void*);
static void*(*org_realloc_hook)(void*, size_t, const void*);
//static void*(*org_memalign_hook)(size_t, size_t, const void*);
//static void(*org_free_hook)(void*, const void*);

int main()
{
  void* p = malloc(100);
  printf("%p\n", p);
  free (p);
  return 0;
}

static void* malloc_hook(size_t size, const void* ra)
{
  __malloc_hook = org_malloc_hook;
  void *p = malloc(size);
  __malloc_hook = malloc_hook;
  return p;
}

static void* realloc_hook(void* p, size_t size, const void* ra)
{
  __realloc_hook = org_realloc_hook;
  p = realloc(p, size);
  __realloc_hook = realloc_hook;
  return p;
}

void init_hook(void)
{
  org_malloc_hook  = __malloc_hook;
  __malloc_hook    = malloc_hook;
  org_realloc_hook = __realloc_hook;
  __realloc_hook   = realloc_hook;
}

void (* volatile __malloc_initialize_hook)(void) = init_hook;
