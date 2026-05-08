
#include <stdio.h>
#define INITIAL_VARIABLES 128
#define INITIAL_MEMORY 1048576
#define HEADER
#define GLOBAL

typedef struct var {
  char name[32];
  void *pointer;
  int length;
  char type[16];
  int isPointer;
  int isArray;
  int arrayLength;
  int isGlobal;
} var;

typedef void* (*handle)(void** args);

typedef struct shellState {
  void *nextVarPointer;
  var variables[INITIAL_VARIABLES];
  int varCounter;
  int binaryCounter;
  int headerBlocksCounter;
  void *memory;
  char *headerBlocks[INITIAL_VARIABLES];
} shellState;

typedef struct shellApi {
  void (*set)(char *, void *, int);
  void (*get)(char *, void *);
  int (*getlen)(char *);
} shellApi;
