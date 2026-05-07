
#include <stdio.h>
#define INITIAL_VARIABLES 128
#define INITIAL_MEMORY 1048576

typedef struct {
  char *name;
  void *pointer;
  int length;
} var;

typedef void* (*handle)(void** args);

typedef struct {
  char *name;
  handle fn;
} fn;

typedef struct {
  void *nextVarPointer;
  var *variables;
  fn *functions;
  int varCounter;
  int functionCounter;
  int binaryCounter;
  void* memory;
} shellState;