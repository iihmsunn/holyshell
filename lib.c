#include <stdio.h>
#include <stdlib.h>
#include "../hs.h"

#define setByValue(TYPE, NAME, VALUE)\
TYPE NAME = VALUE;\
api->set(st, #NAME, &NAME, sizeof(TYPE))

#define setByPointer(TYPE, NAME, VALUE)\
TYPE NAME = VALUE;\
api->set(st, #NAME, NAME, sizeof(TYPE))

#define getValue(TYPE, NAME)\
TYPE NAME;\
api->get(st, #NAME, &NAME)

#define getPointer(TYPE, NAME)\
TYPE NAME;\
api->get(st, #NAME, NAME)

#define call(NAME, ARG1VALUE)\
api->callFunction(st, #NAME, (void*)ARG1VALUE)

int _i = 0;

shellState *st;
shellApi *api;

void __init__(void** args) {
  st = args[0];
  api = args[1];
}
