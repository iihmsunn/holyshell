Minimal effort shell that compiles and executes C code on the fly.

New line adds the current line to the current code block, and sending an empty line executes the block.

Code block gets wrapped into a function and sent to real GCC which builds a shared library that is immediately loaded.

Variable state is persisted in between the block executions.

If the block starts with a HEADER keyword (which should take the entire first line), instead of being executed, it's going to be appended at the top of the code before the next execution, which can be used to define functions, types and add preprocessor directives.

If the block starts with a GLOBAL keyword, all variables from it are declared as global.
