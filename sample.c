#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void func_a()
{
  void *p;
  p = malloc(100);
  printf("[func_a] malloc [%p]\n", p); // leak address
  p = malloc(100);
  printf("[func_a] malloc [%p]\n", p);
  free(p);
  printf("[func_a] free   [%p]\n", p);
}
void func_b()
{
  void *p;
  p = malloc(200);
  printf("[func_b] malloc [%p]\n", p); // leak address
  p = malloc(200);
  printf("[func_b] malloc [%p]\n", p);
  free(p);
  printf("[func_b] free   [%p]\n", p);
}

int main(int argc, char** argv)
{
  char inbuf[32];
  int i = 0;
  while(1) {
    printf("--- input (q:exit)->");
    memset(inbuf, 0, sizeof(inbuf));
    if (!fgets(inbuf, sizeof(inbuf)-1, stdin)) break;
    if (inbuf[0] == 'q') break;

    if (i%2) func_a();
    else func_b();
    i++;
  }
  return 0;
}
