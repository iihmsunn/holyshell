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
#include <readline/readline.h>
#include <readline/history.h>
#include "hs.h"

void get(shellState *st, char *name, void *pointer);
int getlen(shellState *st, char *name);
void set(shellState *st, char *name, void *pointer, int size);
void* callFunction(shellState *st, char *name, void** args);
void registerFunction(char *name, handle handle);
int loadLib(char *path, char *fnName);
void printState();
void printFunctions_host();
void getFunctionName(char* code, char* result);
int compileC(char *code, char *fnName);

char statePath[256];
char CC[128];
char commonLibCode[4096];
regex_t functionNameRegex;

shellState *s;
shellApi *api;

int main() {
  //init regexes
  int regexResult = regcomp(&functionNameRegex, "([[:alnum:]_]+)[[:space:]]?\\(.*\\)[[:space:]]?\\{.*\\}", REG_EXTENDED);
  if (regexResult) {
    fprintf(stderr, "Regex compilation error\n");
    return 1;
  }

  //init state
  s = malloc(sizeof(shellState));
  s->binaryCounter = 0;
  s->functionCounter = 0;
  s->functions = malloc(INITIAL_VARIABLES * sizeof(fn));
  s->memory = malloc(INITIAL_MEMORY);
  s->nextVarPointer = s->memory;
  s->varCounter = 0;
  s->variables = malloc(INITIAL_VARIABLES * sizeof(var));

  api = malloc(sizeof(api));
  api->callFunction = callFunction;
  api->get = get;
  api->getlen = getlen;
  api->set = set;

  //init shared lib code
  FILE *sharedCodeFile;
  int sharedCodeFileLength;
  sharedCodeFile = fopen("lib.c", "r");
  fseek(sharedCodeFile, 0, SEEK_END);
  sharedCodeFileLength = ftell(sharedCodeFile);
  fseek(sharedCodeFile, 0, SEEK_SET);
  int codeReadRes = fread(commonLibCode, sharedCodeFileLength, 1, sharedCodeFile);

  char *codeBlock = malloc(4000 * sizeof(char));
  char *command;

  sprintf(statePath, ".hs_state");
  sprintf(CC, "gcc");
  mkdir(statePath, 0700);

  while ((command = readline(">> ")) != NULL) {
    int execute = 0;

    if (strlen(command) > 0) {
      add_history(command);
    } else {
      if (strlen(codeBlock) > 0) {
        execute = 1;
      }
    }

    if (!execute) {
      strcat(codeBlock, command);

      free(command);
      continue;
    }

    if (!strcmp(codeBlock, "_exit();")) {
      fprintf(stdout, "Exiting...\n");
      break;
    }
    else if (!strcmp(codeBlock, "_state();")) {
      printState();
    }
    else if (!strcmp(codeBlock, "_functions();")) {
      printFunctions_host();
    }
    else {
        char fnName[128] = "";
        getFunctionName(codeBlock, fnName);

        compileC(codeBlock, fnName);
    }

    codeBlock[0] = '\0';
  }

  return 0;
}

int runCommand(char *command, char *data) {
  FILE *pf;

  pf = popen(command, "r");
  char *outputResult = fgets(data, 1024, pf);
  int result = pclose(pf);

  return result;
}

int compileC(char *code, char *fnName) {
  char buffer[512] = "";
  char wrapperStart[512] = "";
  char wrapperEnd[512] = "";

  if (fnName[0] == '\0') {
    sprintf(wrapperStart, "void __temp__() {\n");
    sprintf(wrapperEnd, "\n}\n");
  }

  FILE *codeFile;
  char codeFileName[512] = "";
  char oFileName[512] = "";
  char libFileName[512] = "";
  sprintf(codeFileName, "%s/%d.c", statePath, s->binaryCounter);
  sprintf(oFileName, "%s/%d.o", statePath, s->binaryCounter);
  sprintf(libFileName, "%s/%d.so", statePath, s->binaryCounter);
  codeFile = fopen(codeFileName, "w");
  if (codeFile == (FILE*)-1) {
    fprintf(stderr, " Error: Failed to create a file to write code \n");
  }

  size_t written = fprintf(codeFile, "//common part:\n%s\n\n//main part:\n%s%s%s", commonLibCode, wrapperStart, code, wrapperEnd);
  if (written == -1) {
    fprintf(stderr, " Error: Failed to write code to file \n");
  }
  fclose(codeFile);

  char oCommand[1500] = "";
  char soCommand[1500] = "";
  int oResult = sprintf(oCommand, "%s -c -fpic %s -o %s", CC, codeFileName, oFileName);
  int soResult = sprintf(soCommand, "%s -shared %s -o %s", CC, oFileName, libFileName);
  
  char ccOutput[4096] = "";
  int ccResult = runCommand(oCommand, ccOutput);
  fprintf(stderr, "%s", ccOutput);
  ccOutput[0] = '\0';
  if (!ccResult) {
    ccResult = runCommand(soCommand, ccOutput);
    fprintf(stderr, "%s", ccOutput);
  }

  if (!ccResult) {
    loadLib(libFileName, fnName);
  }

  s->binaryCounter++;
  return oResult;
}

int loadLib(char *path, char *fnName) {
  void *libHandle = dlopen(path, RTLD_LAZY);

  if (!libHandle) {
    fprintf(stderr, "Error: %s\n", dlerror());
    return EXIT_FAILURE;
  }

  handle initHandle = dlsym(libHandle, "__init__");
  if (!initHandle) {
    fprintf(stderr, "Error initializing a library: %s\n", dlerror());
    dlclose(libHandle);
    return EXIT_FAILURE;
  }

  void *args[2];
  args[0] = s;
  args[1] = api;
  initHandle(args);

  handle fnHandle;
  if (fnName[0] == '\0') {
    fnHandle = dlsym(libHandle, "__temp__");
  } else {
    fnHandle = dlsym(libHandle, fnName);
  }
  
  if (!fnHandle) {
    fprintf(stderr, "Error: %s\n", dlerror());
    dlclose(libHandle);
    return EXIT_FAILURE;
  }

  if (fnName[0] == '\0') {
    fnHandle(NULL);
  } else {
    registerFunction(fnName, fnHandle);
  }

  return 0;
}

void registerFunction(char *name, handle handle) {
  char *newName = malloc(strlen(name) * sizeof(char) + 1);
  strcpy(newName, name);
  s->functions[s->functionCounter] = (fn){
    .name = newName,
    .fn = handle
  };
  s->functionCounter++;
}

void printState() {
  var *var;
  fprintf(stdout, "Attempting to print variables as ints...:\n");
  for (int i = 0; i < s->varCounter; i++) {
    var = &s->variables[i];
    fprintf(stdout, "%s: %d\n", var->name, *(int*)var->pointer);
  }
}

void printFunctions_host() {
  fn *fn;
  fprintf(stdout, "Attempting to print functions...:\n");
  for (int i = 0; i < s->functionCounter; i++) {
    fn = &s->functions[i];
    fprintf(stdout, "%s\n", fn->name);
  }
}

void getFunctionName(char* code, char* result) {
  size_t ngroups = functionNameRegex.re_nsub + 1;
  regmatch_t *groups = malloc(ngroups * sizeof(regmatch_t));
  int execResult = regexec(&functionNameRegex, code, ngroups, groups, 0);

  // size_t nmatched;
  regmatch_t *group = &groups[1];
  if (group->rm_so >= 0
      && group->rm_so < group->rm_so < group->rm_eo
  ) {
      int length = group->rm_eo - group->rm_so; 
      strncpy(result, code + group->rm_so, length);
  }

  free(groups);
}

void set(shellState *st, char *name, void *pointer, int size) {
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

void get(shellState *st, char *name, void *pointer) {
  var *var;
  for (int i = 0; i < st->varCounter; i++) {
    var = &st->variables[i];
    if (!strcmp(var->name, name)) {
      memcpy(pointer, var->pointer, var->length);
    }
  }
}

int getlen(shellState *st, char *name) {
  var *var;
  for (int i = 0; i < st->varCounter; i++) {
    var = &st->variables[i];
    if (!strcmp(var->name, name)) {
      return var->length;
    }
  }

  return 0;
}

void* callFunction(shellState *st, char *name, void** args) {
  fn *fn;
  for (int i = 0; i < st->functionCounter; i++) {
    fn = &st->functions[i];
    if (!strcmp(fn->name, name)) {
      return fn->fn(args);
    }
  }

  return NULL;
}