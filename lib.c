#include <stdio.h>
#include <stdlib.h>
#include "../hs.h"

shellState *st;
shellApi *api;

void __init__(void** args) {
  st = args[0];
  api = args[1];
}
