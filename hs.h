
#include <stdio.h>
#define INITIAL_VARIABLES 128
#define INITIAL_MEMORY 1048576

typedef struct {
  char *name;
  void *pointer;
  int length;
} var;

typedef void* (*handle)(va_list args);

typedef struct {
  char *name;
  handle fn;
} fn;
