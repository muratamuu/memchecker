#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <sys/types.h>
#include <malloc.h>
#include <map>
#include <mutex>
#include <memory>
#include <stdint.h>

using namespace std;

static void*(*org_malloc_hook)(size_t, const void*);
static void*(*org_realloc_hook)(void*, size_t, const void*);
static void*(*org_memalign_hook)(size_t, size_t, const void*);
static void(*org_free_hook)(void*, const void*);

// memory context
struct memctx
{
  void* adr;
  size_t size;
  const void* ra;
  memctx(void* adr, size_t size, const void* ra)
    : adr(adr), size(size), ra(ra) {}
};

// global variables
static mutex m;
static map<void*, memctx*> memtbl;

static void addctx(void* adr, size_t size, const void* ra)
{
  if ((memtbl[adr] = new memctx(adr, size, ra)) == nullptr)
    perror("addctx error");
}

static void delctx(void* adr)
{
  auto* ctx = memtbl[adr];
  if (ctx) {
    delete ctx;
    memtbl.erase(adr);
  }
}

static void dispctx()
{
  if (!memtbl.empty())
    printf("|-- ADDRESS -|--- SIZE ---|---- RA ----|\n");

  for (auto elm : memtbl)
    printf("| 0x%08lx | 0x%08lx | 0x%08lx |\n",
           (uintptr_t)elm.second->adr, elm.second->size, (uintptr_t)elm.second->ra);
}

static void* malloc_hook(size_t size, const void* ra)
{
  lock_guard<mutex> lock(m);
  __malloc_hook = org_malloc_hook;
  void *p = malloc(size);
  if (p) addctx(p, size, ra);
  //printf("malloc hook\n");
  __malloc_hook = malloc_hook;
  return p;
}

static void* realloc_hook(void* p, size_t size, const void* ra)
{
  lock_guard<mutex> lock(m);
  __realloc_hook = org_realloc_hook;
  p = realloc(p, size);
  if (p) addctx(p, size, ra);
  //printf("realloc hook\n");
  __realloc_hook = realloc_hook;
  return p;
}

static void* memalign_hook(size_t alignment, size_t size, const void* ra)
{
  lock_guard<mutex> lock(m);
  __memalign_hook = org_memalign_hook;
  void* p = memalign(alignment, size);
  if (p) addctx(p, size, ra);
  //printf("memalign hook\n");
  __memalign_hook = memalign_hook;
  return p;
}

static void free_hook(void* p, const void* ra)
{
  lock_guard<mutex> lock(m);
  __free_hook = org_free_hook;
  free(p);
  if (p) delctx(p);
  //printf("free hook\n");
  __free_hook = free_hook;
  return;
}

static void init_hook(void)
{
  org_malloc_hook   = __malloc_hook;
  __malloc_hook     = malloc_hook;
  org_realloc_hook  = __realloc_hook;
  __realloc_hook    = realloc_hook;
  org_memalign_hook = __memalign_hook;
  __memalign_hook   = memalign_hook;
  org_free_hook     = __free_hook;
  __free_hook       = free_hook;
}

extern "C" {
void (* volatile __malloc_initialize_hook)(void) = init_hook;
}

static struct _caller {
  _caller() {}
  ~_caller() { dispctx(); }
} caller;
