#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <regex.h>
#include "hs.h"

void get(char *name, void *pointer);
int getlen( char *name);
void set(char *name, void *pointer, int size);
int loadLib(char *path);
void printState();
int compileC(char *code);
void getVariables(char *code, int global);
regex_t declarationRegex;
const char* declarationRegexString = "(^|\n)([[:alpha:]_]+[[:blank:]*]+)([[:alnum:]_]+)(\\[[[:digit:]]+\\])?";

char statePath[256];
char CC[128];
char commonLibCode[4096];

char headerCode[4096];
char restoreVars[4096];
char persistVars[4096];
char globalVars[4096];

shellState *s;
shellApi *api;

int main() {
  //init regexes
  int regexResult = regcomp(&declarationRegex, declarationRegexString, REG_EXTENDED);
  if (regexResult) {
    fprintf(stderr, "Regex compilation error\n");
    return 1;
  }
  
  //init state
  s = malloc(sizeof(shellState));
  s->binaryCounter = 0;
  s->memory = malloc(INITIAL_MEMORY);
  s->nextVarPointer = s->memory;
  s->varCounter = 0;

  api = malloc(sizeof(shellApi));
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

  char codeBlock[4000] = "";
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
      strcat(codeBlock, "\n");

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
    else {
        compileC(codeBlock);
    }

    codeBlock[0] = '\0';
    free(command);
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

int compileC(char *code) {
  char buffer[512] = "";
  char wrapperStart[512] = "";
  char wrapperEnd[512] = "";

  sprintf(wrapperStart, "void __temp__() {\n");
  sprintf(wrapperEnd, "\n}\n");

  FILE *codeFile;
  char codeFileName[512] = "";
  char oFileName[512] = "";
  char libFileName[512] = "";
  headerCode[0] = '\0';
  restoreVars[0] = '\0';
  persistVars[0] = '\0';
  globalVars[0] = '\0';
  sprintf(codeFileName, "%s/%d.c", statePath, s->binaryCounter);
  sprintf(oFileName, "%s/%d.o", statePath, s->binaryCounter);
  sprintf(libFileName, "%s/%d.so", statePath, s->binaryCounter);

  for (int i = 0; i < s->headerBlocksCounter; i++) {
    strcat(headerCode, s->headerBlocks[i]);
    strcat(headerCode, "\n");
  }

  var *var;
  for (int i = 0; i < s->varCounter; i++) {
    var = &s->variables[i];

    //declare
    if (!var->isGlobal) {
      sprintf(restoreVars + strlen(restoreVars), "%s%s", var->type, var->name);
      if (var->isArray) {
        sprintf(restoreVars + strlen(restoreVars), "[%d]", var->arrayLength);
      }
      sprintf(restoreVars + strlen(restoreVars), ";\n");
    } else {
      sprintf(globalVars + strlen(globalVars), "%s%s", var->type, var->name);
      if (var->isArray) {
        sprintf(globalVars + strlen(globalVars), "[%d]", var->arrayLength);
      }
      sprintf(globalVars + strlen(globalVars), ";\n");
    }

    //restore value
    if (var->isPointer) {
      sprintf(restoreVars + strlen(restoreVars), 
        "api->get(\"%s\", %s);\n", var->name, var->name);
    } else {
      sprintf(restoreVars + strlen(restoreVars), 
        "api->get(\"%s\", &%s);\n", var->name, var->name);
    }

    //persist
    if (var->isPointer) {
      sprintf(persistVars + strlen(persistVars),
        "api->set(\"%s\", %s, sizeof(%s)*%d);\n", var->name, var->name, var->type, var->arrayLength);
    } else {
      sprintf(persistVars + strlen(persistVars), 
        "api->set(\"%s\", &%s, sizeof(%s)*%d);\n", var->name, var->name, var->type, var->arrayLength);
    }
  }

  codeFile = fopen(codeFileName, "w");
  if (codeFile == (FILE*)-1) {
    fprintf(stderr, " Error: Failed to create a file to write code \n");
  }

  const char *template =
    "//common part:\n"
    "%s\n\n"
    "//global variables:\n"
    "%s\n\n"
    "//header part:\n"
    "%s\n\n"
    "%s" //wrapper opener
    "//restoring variables:\n"
    "%s\n\n"
    "//main part:\n"
    "%s\n\n" //code
    "//persisting variables:\n"
    "%s\n\n"
    "%s"; //wrapper closer

  size_t written = fprintf(codeFile, template, commonLibCode, globalVars, headerCode, wrapperStart, restoreVars, code, persistVars, wrapperEnd);
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

  int isHeaderBlock = strncmp("HEADER\n", code, 7) == 0;
  int isGlobalBlock = strncmp("GLOBAL\n", code, 7) == 0;

  if (!ccResult) {

    if (isHeaderBlock) {
      //don't run anything but append the code to header blocks
      char *block = malloc(sizeof(char)*strlen(code) + 1);
      strcpy(block, code);
      s->headerBlocks[s->headerBlocksCounter++] = block;
    } else {
      //extract variables and run the code
      if (!isHeaderBlock) {
        getVariables(code, isGlobalBlock);
      }
      loadLib(libFileName);
    }
  }

  s->binaryCounter++;
  
  return oResult;
}

int loadLib(char *path) {
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

  void *args[1] = { api };
  initHandle(args);

  handle fnHandle;
  fnHandle = dlsym(libHandle, "__temp__");
  
  if (!fnHandle) {
    fprintf(stderr, "Error: %s\n", dlerror());
    dlclose(libHandle);
    return EXIT_FAILURE;
  }

  fnHandle(NULL);

  return 0;
}

void printState() {
  var *var;
  fprintf(stdout, "Attempting to print variables as ints...:\n");
  for (int i = 0; i < s->varCounter; i++) {
    var = &s->variables[i];
    fprintf(stdout, "%s: %d\n", var->name, *(int*)var->pointer);
  }
}

void set(char *name, void *pointer, int size) {
  if (pointer == NULL) return;

  int index = -1;
  for (int i = 0; i < s->varCounter; i++) {
    if (!strcmp(s->variables[i].name, name)) {
      index = i;
      break;
    }
  }

  if (index == -1) return;

  var *var = &s->variables[index];
  if (var->pointer == NULL) {
    var->pointer = s->nextVarPointer;
    s->nextVarPointer = s->nextVarPointer + size;
  }
  
  memcpy(var->pointer, pointer, size);
  var->length = size;
}

void get(char *name, void *pointer) {
  if (pointer == NULL) return;

  var *var;
  for (int i = 0; i < s->varCounter; i++) {
    var = &s->variables[i];
    if (!strcmp(var->name, name)) {
      memcpy(pointer, var->pointer, var->length);
    }
  }
}

int getlen(char *name) {
  var *var;
  for (int i = 0; i < s->varCounter; i++) {
    var = &s->variables[i];
    if (!strcmp(var->name, name)) {
      return var->length;
    }
  }

  return 0;
}

void getVariables(char *code, int global) {
  int offset = 0;
  size_t ngroups = declarationRegex.re_nsub + 1;
  regmatch_t *groups = malloc(ngroups * sizeof(regmatch_t));

  while (1) {
    // "^([[:alpha:]_]+(?=[[:space:]*])[[:space:]]*[*]?[[:space:]]*)([[:alnum:]_]+)(\\[[[:digit:]]+\\])?"
    int execResult = regexec(&declarationRegex, code + offset, ngroups, groups, 0);
    if (execResult != 0) break;

    regmatch_t *match = &groups[0];
    regmatch_t *gType = &groups[2];
    regmatch_t *gName = &groups[3];
    regmatch_t *gArray = &groups[4];

    var *var = &s->variables[s->varCounter++];
    strncpy(var->name, code + gName->rm_so + offset, gName->rm_eo - gName->rm_so);
    strncpy(var->type, code + gType->rm_so + offset, gType->rm_eo - gType->rm_so);

    var->isGlobal = global;

    for (int i = 0; i < strlen(var->type); i++) {
      if (var->type[i] == '*') {
        var->isPointer = 1;
        break;
      }
    }

    if (gArray->rm_so > -1) {
      var->isArray = 1;
      char* digits = malloc(sizeof(char) * (gArray->rm_eo - gArray->rm_so));
      strncpy(digits, code + gArray-> rm_so + offset + 1, gArray->rm_eo - gArray->rm_so - 1);
      var->arrayLength = atoi(digits);
      free(digits);
    } else {
      var->arrayLength = 1;
    }

    offset = match->rm_eo + offset + 1;
  }
  
  free(groups);  
}
