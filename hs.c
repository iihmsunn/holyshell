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
#include "hs.h"

int runCommand(char *command, char *data) {
  FILE *pf;

  pf = popen(command, "r");
  char *outputResult = fgets(data, 1024, pf);
  if (!outputResult) {
    fprintf(stderr, " Error: Failed to read command output \n"); 
  }
  
  int result = pclose(pf);
  if (result != 0) {
    fprintf(stderr, " Error: Failed to close command stream \n");
  }

  return result;
}

int loadLib(char *path, char *fnName);

int binaryCounter = 0;
int varCounter = 0;
int functionCounter = 0;
char statePath[256];
char CC[128];
void* memory;
var variables[INITIAL_VARIABLES];
fn functions[INITIAL_VARIABLES];
void *nextVarPointer;
regex_t functionNameRegex;

void registerFunction(char *name, handle handle) {
  functions[functionCounter] = (fn){
    .name = name,
    .fn = handle
  };
  functionCounter++;
}

int compileC(char *code, int length, char *fnName) {
  ssize_t n;
  char buffer[512];
  int codeFile;
  char codeFileName[256];
  char libFileName[256];
  sprintf(codeFileName, "%s/%d.c", CC, binaryCounter);
  sprintf(libFileName, "%s/%d.so", CC, binaryCounter);
  codeFile = open(codeFileName, O_WRONLY);

  ssize_t written = write(codeFile, code, length);
  if (written == -1) {
    fprintf(stderr, " Error: Failed to write to file \n");
  }
  close(codeFile);

  char command[1024];
  int result = sprintf(command, "%s %s -o %s", CC, codeFileName, libFileName);

  loadLib(libFileName, fnName);
  
  binaryCounter++;
  return result;
}

int loadLib(char *path, char *fnName) {
  void *libHandle = dlopen(path, RTLD_LAZY);

  if (!libHandle) {
    fprintf(stderr, "Error: %s\n", dlerror());
    return EXIT_FAILURE;
  }
  
  handle fnHandle = dlsym(libHandle, fnName);
  if (!fnHandle) {
    fprintf(stderr, "Error: %s\n", dlerror());
    dlclose(libHandle);
    return EXIT_FAILURE;
  }

  if (!strcmp(fnName, "__temp__")) {
    fnHandle(NULL);
  } else {
    registerFunction(fnName, fnHandle);
  }

  return 0;
}

void printState() {
  var *var;
  printf("Attempting to print variables as ints...:\n");
  for (int i = 0; i < varCounter; i++) {
    var = &variables[i];
    printf("%s: %d\n", var->name, *(int*)var->pointer);
  }
}

void getFunctionName(char* code, char* result) {
  size_t ngroups = functionNameRegex.re_nsub + 1;
  regmatch_t *groups = malloc(ngroups * sizeof(regmatch_t));
  int execResult = regexec(&functionNameRegex, code, ngroups, groups, 0);

  size_t nmatched;
  for (nmatched = 0; nmatched < ngroups; nmatched++) {
    regmatch_t *group = &groups[nmatched];
    if (group->rm_so == (size_t)(-1)) {
      break;
    } else {
      char substr[(group->rm_so - group->rm_eo)/8];
      strncpy(code, code + group->rm_eo / 8, (group->rm_so - group->rm_eo)/8);
      printf("%s", substr);
      result = substr;
    }
  }
  
  free(groups);
}

int main() {
  // int regexResult = regcomp(&functionNameRegex, "([[:alnum:]_]+)[[:space:]]?\\(.+\\)[[:space:]]?\\{.*\\}", 0);
  int regexResult = regcomp(&functionNameRegex, "([[:alnum:]_]+)[[:space:]]?\\(.*\\)[[:space:]]?{.*}", 0);
  if (regexResult) {
    printf("Regex compilation error\n");
    return 1;
  }
  
  memory = malloc(INITIAL_MEMORY);
  nextVarPointer = memory;
  char command[1024];
  
  sprintf(statePath, ".hs_state");
  sprintf(CC, "gcc");
  mkdir(statePath, 0700);

  while (1) {
    int matched = scanf("%s", command);
    printf("command: %s\n", command);
    if (!strcmp(command, "_exit();")) {
      printf("Exiting...\n");
      break;
    }
    else if (!strcmp(command, "_state()")) {
      printState();
    }

    char* fnName = malloc(sizeof(char) * 128);
    getFunctionName(command, fnName);
    if (fnName) {
      printf("fnName: %s", fnName);
    }

    compileC(command, 1024, "__temp__");
  }

  return 0;
}
