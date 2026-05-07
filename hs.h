
#include <stdio.h>
#define INITIAL_VARIABLES 128
#define INITIAL_MEMORY 1048576

typedef struct var {
  char *name;
  void *pointer;
  int length;
} var;

typedef void* (*handle)(void** args);

typedef struct fn {
  char *name;
  handle fn;
} fn;

typedef struct shellState {
  void *nextVarPointer;
  var *variables;
  fn *functions;
  int varCounter;
  int functionCounter;
  int binaryCounter;
  void* memory;
} shellState;

typedef struct shellApi {
  void (*set)(shellState *, char *, void *, int);
  void (*get)(shellState *, char *, void *);
  int (*getlen)(shellState *, char *);
  void *(*callFunction)(shellState *, char*, void**);
} shellApi;