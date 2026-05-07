Minimal effort shell that compiles and executes C code on the fly.
New line adds the current line to the current code block, and sending an empty line executes the block.
Code block gets wrapped into a function and sent to real GCC which builds a shared library that is immediately loaded.

If the code has a function definition, the compiled function gets stored for later use, otherwise the code is just immediately executed.
But to call function compiled in a previous call, a special api has to be used.
Same with variables, to persist them between different blocks, a special api has to be used, normal variables are not persisted.

I may or may not later try to implement something that automatically adds those api calls so that normal C variables are persisted.
