#include "hs.h"
#include <stdarg.h>

extern void* nextVarPointer;
extern var variables[INITIAL_VARIABLES];
extern fn functions[INITIAL_VARIABLES];
extern int varCounter;
extern int functionCounter;

#include <string.h>
void set(char *name, void *pointer, int size) {
  memcpy(nextVarPointer, pointer, size);

  variables[varCounter] = (var){
    .length = size,
    .name = name,
    .pointer = nextVarPointer 
  };

  nextVarPointer = nextVarPointer + size;

  varCounter++;
}

void get(char *name, void *pointer) {
  var *var;
  for (int i = 0; i < varCounter; i++) {
    var = &variables[i];
    if (var->name == name) {
      memcpy(pointer, var->pointer, var->length);
    }
  }
}

void callFunction(char *name, ...) {
  fn *fn;
  for (int i = 0; i < functionCounter; i++) {
    fn = &functions[i];
    if (fn->name == name) {
      va_list args;
      va_start(args, name);
      fn->fn(args);
      va_end(args);
    }
  }
}
