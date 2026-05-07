#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <regex.h>
#include "../hs.h"

shellState *st;

void set(char *name, void *pointer, int size) {
  memcpy(st->nextVarPointer, pointer, size);
  char *newName = malloc(strlen(name) * sizeof(char) + 1);
  strcpy(newName, name);

  st->variables[st->varCounter] = (var){
    .length = size,
    .name = newName,
    .pointer = st->nextVarPointer 
  };

  st->nextVarPointer = st->nextVarPointer + size;

  st->varCounter++;
}

void get(char *name, void *pointer) {
  var *var;
  for (int i = 0; i < st->varCounter; i++) {
    var = &st->variables[i];
    if (!strcmp(var->name, name)) {
      memcpy(pointer, var->pointer, var->length);
    }
  }
}

int getlen(char *name) {
  var *var;
  for (int i = 0; i < st->varCounter; i++) {
    var = &st->variables[i];
    if (!strcmp(var->name, name)) {
      return var->length;
    }
  }

  return 0;
}

void callFunction(char *name, void** args) {
  fn *fn;
  for (int i = 0; i < st->functionCounter; i++) {
    fn = &st->functions[i];
    if (!strcmp(fn->name, name)) {
      fn->fn(args);
    }
  }
}

void __init__(void** args) {
  st = args[0];
}
