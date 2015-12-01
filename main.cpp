#include <stdio.h>
#include <malloc.h>
#include <mutex>
#include <memory>

static void*(*org_malloc_hook)(size_t, const void*);
static void*(*org_realloc_hook)(void*, size_t, const void*);
static void*(*org_memalign_hook)(size_t, size_t, const void*);
static void(*org_free_hook)(void*, const void*);

using namespace std;
mutex m_;

class A
{
private:
  int a_;
public:
  A(int a):a_(a){}
  ~A(){}
  void disp() { printf("%d\n", a_); }
};

int main()
{
  //void* p = malloc(100);
  //printf("%p\n", p);
  //free (p);
  unique_ptr<A> obj(new A(10));
  obj->disp();
  return 0;
}

static void* malloc_hook(size_t size, const void* ra)
{
  lock_guard<mutex> lock(m_);
  __malloc_hook = org_malloc_hook;
  void *p = malloc(size);
  printf("malloc hook\n");
  __malloc_hook = malloc_hook;
  return p;
}

static void* realloc_hook(void* p, size_t size, const void* ra)
{
  lock_guard<mutex> lock(m_);
  __realloc_hook = org_realloc_hook;
  p = realloc(p, size);
  printf("realloc hook\n");
  __realloc_hook = realloc_hook;
  return p;
}

static void* memalign_hook(size_t alignment, size_t size, const void* ra)
{
  lock_guard<mutex> lock(m_);
  __memalign_hook = org_memalign_hook;
  void* p = memalign(alignment, size);
  printf("memalign hook\n");
  __memalign_hook = memalign_hook;
  return p;
}

static void free_hook(void* p, const void* ra)
{
  lock_guard<mutex> lock(m_);
  __free_hook = org_free_hook;
  free(p);
  printf("free hook\n");
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
