#include <stdio.h>
#include <stdlib.h>
#include "../hs.h"

shellApi *api;

void __init__(void** args) {
  api = args[0];
}
