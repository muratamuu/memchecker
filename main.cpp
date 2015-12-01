#include <stdio.h>
#include <malloc.h>

int main()
{
  void* p = malloc(100);
  printf("%p\n", p);

  return 0;
}
