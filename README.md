# Tea language compiler: Stage 3 (Custom Features, AST Optimizer & Native Program)

A statically-typed programming language featuring Ukrainian keywords, a tea-inspired domain design, an AST optimizer, native code generation using LLVM IR (`llvm::IRBuilder`), and POSIX multi-threading support.

## Language Overview & Syntax Reference

| Concept | Keyword / Symbol | Meaning / Description | 
| ----- | ----- | ----- | 
| 32-bit Integer | `горнятко_32чл` | `i32` standard signed integer | 
| 64-bit Integer | `кухоль_64чл` | `i64` wide signed integer | 
| Boolean | `заповненість` | `bool` logical type | 
| Thread Handle | `потік` | Thread reference type for concurrency | 
| Mutable Marker | `відкрита` | Specifies variable or field mutability | 
| Global Constant | `факт` | Compile-time global constant | 
| True / False | `повне` / `пусте` | Boolean literals | 
| Conditional | `якщо` / `інакше` | `if` / `else` conditional branch | 
| While Loop | `допоки` | Condition-driven loop | 
| For Loop | `для` | Range/counting loop (`для (init; cond; step)`) | 
| Thread Spawn | `витечи` | Spawns a global function on a new pthread | 
| Thread Join | `дочекатись` | Synchronizes and waits for thread completion | 
| Screen Clear | `clear()` | Erases terminal buffer (ANSI sequence) | 
| Program Exit | `чайна_перерва` | Returns from function or exits program with status | 
| Declaration | `:=` | `type name := value` | 
| Assignment | `=>` | Reversed assignment: `value => target` | 
| Structure | `struct` | Composite user-defined type with methods | 
| Functions | `fn`, `->` | `fn name := (params) -> type` | 
| Member Access | `.` | Accesses struct fields and methods (`obj.field`, `obj.method()`) | 
| I/O Built-ins | `read()`, `print(...)` | Console input and output | 
| Comments | `#` | Inline comment to end of line | 

## Compiler Usage & CLI Flags

```bash
# Build the compiler executable
mkdir -p build && cd build
cmake .. && cmake --build . --target compiler
cd ..

# 1. Compile source to LLVM IR
./build/compiler input.txt output.ll

# 2. Print initial Abstract Syntax Tree (AST)
./build/compiler --ast input.txt

# 3. Compile with AST Optimizations enabled
./build/compiler --optimize input.txt output.ll

# 4. Print AST after optimization passes
./build/compiler --optimize --ast input.txt
```

### Native Binary Compilation (llc + clang)

To compile the generated LLVM IR into a native binary executable:

```bash
# Compile LLVM IR to object code
llc -filetype=obj output.ll -o output.o

# Link into a native executable (linking POSIX threads)
clang output.o -pthread -o tea_program

# Run native executable
./tea_program
```

## Task 1: Custom Feature — Threads & Loop Constructs

TeaLang implements POSIX multi-threading (`pthread`) and `для` (range/counting) loops.

### Syntax (EBNF Excerpt)
```ebnf
for_stmt    ::= "для" "(" [ for_init ] ";" [ expression ] ";" [ for_step ] ")" NL block
for_init    ::= decl_core
for_step    ::= assign_core

thread_decl ::= "потік" identifier NL
spawn_stmt  ::= "витечи" call "=>" identifier NL
join_stmt   ::= "дочекатись" identifier NL
```

### Feature Rules & Semantics
1. **Threads (`потік`, `витечи`, `дочекатись`):**
   * Thread handles are declared with `потік handle_name`.
   * `витечи fn_name(args) => handle` evaluates arguments in the spawner thread, allocates a thread payload structure on the heap, and launches `pthread_create`.
   * `дочекатись handle` blocks execution via `pthread_join` until the thread finishes and frees allocated resources.
   * Variables modified concurrently across two threads without atomic primitives result in undefined memory behavior (data race).

2. **Loops (`для`):**
   * Lowered directly into LLVM IR basic blocks with explicit back-edges.
   * Omitted loop condition defaults to `повне` (infinite execution loop).

### Feature Code Examples
#### Example 1: Parallel Execution with Threads
```text
fn worker := (горнятко_32чл id) -> горнятко_32чл
{
    print(id)
    чайна_перерва id
}

потік t1
потік t2
витечи worker(10) => t1
витечи worker(20) => t2
дочекатись t1
дочекатись t2
чайна_перерва 0
```

### Task 1 IR Excerpt & Explanation
```llvm
%0 = call ptr @malloc(i64 4)
store i32 10, ptr %0, align 4
%1 = call i32 @pthread_create(ptr %t1, ptr null, ptr @worker_thread_wrapper, ptr %0)
%2 = load i64, ptr %t1, align 8
%3 = call i32 @pthread_join(i64 %2, ptr null)
```
* Line 1–2 allocates heap storage for the function arguments, copying parameter values before thread launch to prevent stack races.
* Line 3 calls `pthread_create`, passing the low-level wrapper function `@worker_thread_wrapper` alongside the argument pointer `%0`.
* Line 4–5 loads the thread identifier from handle `%t1` and issues `pthread_join` to block the calling thread until thread completion.

## Task 2: The AST Optimizer

When passed `--optimize`, the compiler executes three AST-level transformations before IR generation:
1. **Unused Variables Removal (`var_remover`):** Removes local declarations and assignments whose values are never referenced. Iterates until convergence. Expressions containing function calls or `read()` are retained for side-effects.
2. **Unused Functions Removal (`fn_remover`):** Prunes top-level functions that are never referenced by any call site.
3. **Function Inlining (`inliner`):** Replaces single-call non-recursive functions directly at the call site.

### Inlining LLVM IR Excerpt (Before vs. After)

**Before Optimization (Unoptimized IR)**
```llvm
define i32 @fn.compute(i32 %x) {
entry:
  %add = add i32 %x, 5
  ret i32 %add
}

define i32 @main() {
entry:
  %call = call i32 @fn.compute(i32 10)
  ret i32 %call
}
```

**After Optimization (`--optimize` IR)**
```llvm
define i32 @main() {
entry:
  %x_inline0 = alloca i32, align 4
  store i32 10, ptr %x_inline0, align 4
  %0 = load i32, ptr %x_inline0, align 4
  %add_inline = add i32 %0, 5
  ret i32 %add_inline
}
```

## Task 3: Interactive & Non-Deterministic Programs (`programs/`)

Certain test programs from the `tests/opt/` suite produce non-deterministic output (due to multithreading) or require interactive user input (`read()`). These have been extracted to the `programs/` directory and must be compiled and run manually.

### 1. The Teacup Animation (`teacup.txt`)
An interactive animated teacup renderer. It prompts the user for animation speed settings, clears the screen buffer, and continuously renders a spinning animated ASCII teacup using custom structures, member functions, clear buffers, and infinite loops.
```bash
./build/compiler --optimize programs/teacup.txt build/teacup.ll
llc -filetype=obj build/teacup.ll -o build/teacup.o
clang build/teacup.o -pthread -o build/teacup
./build/teacup < programs/teacup.in
```

### 2. The Guessing Game (`prog_guess.txt`)
An interactive number guessing game demonstrating loops, conditionals, and standard input.
```bash
./build/compiler --optimize programs/prog_guess.txt build/prog_guess.ll
llc -filetype=obj build/prog_guess.ll -o build/prog_guess.o
clang build/prog_guess.o -pthread -o build/prog_guess

# Run interactively, or feed the pre-recorded input:
./build/prog_guess < programs/prog_guess.in
```

### 3. Collatz Conjecture (`prog_collatz.txt`)
Computes the Collatz sequence interactively based on user input.
```bash
./build/compiler --optimize programs/prog_collatz.txt build/prog_collatz.ll
llc -filetype=obj build/prog_collatz.ll -o build/prog_collatz.o
clang build/prog_collatz.o -pthread -o build/prog_collatz

# Run interactively, or feed the pre-recorded input:
./build/prog_collatz < programs/prog_collatz.in
```

### 4. Parallel Sum Threading (`thread_parallel_sum8.txt`)
Demonstrates 8 parallel threads calculating values concurrently. Output is unpredictable/non-deterministic based on the OS thread scheduler.
```bash
./build/compiler --optimize programs/thread_parallel_sum8.txt build/thread_parallel_sum8.ll
llc -filetype=obj build/thread_parallel_sum8.ll -o build/thread_parallel_sum8.o
clang build/thread_parallel_sum8.o -pthread -o build/thread_parallel_sum8
./build/thread_parallel_sum8
```