#include <iostream>
#include <malloc.h>

using namespace std;

static void*(*org_malloc_hook)(size_t, const void*);

void* malloc_hook(size_t size, const void* ra)
{
  void *p = NULL;
  __malloc_hook = org_malloc_hook;
  p = malloc(size);
  //if (p) addctx(p, size, ra);
  __malloc_hook = malloc_hook;
  printf("Hello world\n");
  return p;
}

int ggg = 0;
void hook_init(void)
{
  org_malloc_hook = __malloc_hook;
  __malloc_hook = malloc_hook;
  ggg = 1;
}

int main()
{
  void* p = malloc(100);
  free(p);
  printf("%d\n", ggg);
  return 0;
}

// weak symbol
extern "C" {
  //void (* volatile __malloc_initialize_hook)(void) = hook_init;
}
