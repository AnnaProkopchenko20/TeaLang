# Tea language compiler: Stage 3 (Threads Feature, AST Optimizer & Native Program)

A statically-typed programming language featuring Ukrainian keywords, a tea-inspired domain design, an AST optimizer, native code generation using LLVM IR (`llvm::IRBuilder`), and POSIX multi-threading support.

## Language Overview & Syntax Reference

| Concept | Keyword / Symbol | Meaning / Description |
| --- | --- | --- |
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

---

## Detailed Documentation of All Added Features

### 1. Threads (`потік`, `витечи`, `дочекатись`)
* **Syntax & Keyword Definitions:**
  * `потік handle_name` declares a dedicated thread handle variable. Thread handles cannot be copied, assigned directly, stored in struct fields, or passed as standard function arguments.
  * `витечи fn_name(args) => handle_name` spawns a global function in a background POSIX thread (`pthread_create`). Arguments are evaluated first in the spawner thread and allocated into a heap payload buffer.
  * `дочекатись handle_name` synchronizes execution by calling `pthread_join` on the handle, blocking until the worker finishes, and freeing allocated heap memory.
* **Error Rules:**
  * Spawning non-functions or passing incorrect argument counts/types triggers a compile-time error.
  * Attempting `дочекатись` on a non-thread handle or using `потік` variables in arithmetic yields a semantic error.
* **Concurrency Data Race Policy:**
  * If two or more threads concurrently write to the same shared variable without external synchronization primitives, a data race occurs, resulting in undefined behavior.

### 2. For Loops (`для`)
* **Syntax:** `для ([for_init] ; [expression] ; [for_step]) NL block`
  * `for_init`: Variable declaration (`decl_core`), e.g., `відкрита горнятко_32чл i := 0`.
  * `expression`: Loop condition, evaluated before each iteration. Omitted condition defaults to `повне` (infinite loop).
  * `for_step`: Variable assignment (`assign_core`), e.g., `i + 1 => i`.
* **Lowering & IR Generation:**
  * Lowered into basic blocks: `for.cond`, `for.body`, `for.step`, `for.end` with explicit conditional branches and back-edges.
* **Scope Rules:**
  * Variables declared in `for_init` are scoped strictly to the loop (condition, step, and body block).

### 3. Screen Clear (`clear()`)
* **Syntax:** `clear() NL`
* **Semantics:** Emits ANSI escape sequence `\033[2J\033[H` to stdout, clearing the terminal buffer and resetting cursor position to top-left.
* **Error Rules:** Cannot take arguments (`clear(x)` is invalid) and cannot be embedded in expressions (`x + clear()`).

---

## Compiler Usage & CLI Flags

```bash
# Build the compiler executable
mkdir -p build && cd build
cmake .. && cmake --build . --target compiler
cd ..

# 1. Compile source to LLVM IR
./build/compiler programs/teacup.txt build/teacup.ll

# 2. Print initial AST
./build/compiler --ast programs/teacup.txt

# 3. Compile with AST Optimizations
./build/compiler --optimize programs/teacup.txt build/teacup.ll

# 4. Print AST after optimization passes
./build/compiler --optimize --ast programs/teacup.txt
```

### Native Binary Compilation (llc + clang)

```bash
# Compile LLVM IR to Position-Independent Object Code
llc -relocation-model=pic -filetype=obj build/teacup.ll -o build/teacup.o

# Link into native binary executable with POSIX threads
clang build/teacup.o -pthread -o build/teacup

# Run interactive native executable
./build/teacup < programs/teacup.in
```

---

## Feature Example Programs

### 1. Thread Example (`tests/ok/thread_basic_spawn_join.txt`)
```tea
fn worker := (горнятко_32чл id) -> горнятко_32чл
{
    print(id)
    чайна_перерва id
}

потік t1
витечи worker(100) => t1
дочекатись t1
чайна_перерва 0
```

### 2. Range Loop Example (`tests/ok/for_simple.txt`)
```tea
відкрита горнятко_32чл sum := 0
для (відкрита горнятко_32чл i := 0; i != 5; i + 1 => i)
{
    sum + i => sum
}
print(sum)
чайна_перерва 0
```

### 3. Screen Clear Example (`tests/ok/clear_statement.txt`)
```tea
clear()
print("Screen cleared")
чайна_перерва 0
```

---

## Task 1 IR Excerpt & Explanation (Threads Argument Packing)
```llvm
%0 = call ptr @malloc(i64 8)
store i32 42, ptr %0, align 4
%1 = getelementptr i8, ptr %0, i64 4
store i32 100, ptr %1, align 4
%2 = call i32 @pthread_create(ptr %t1, ptr null, ptr @worker_thread_wrapper, ptr %0)
%3 = load i64, ptr %t1, align 8
%4 = call i32 @pthread_join(i64 %3, ptr null)
ret i32 0
```
* **Lines 1–4 (`malloc` & `store`):** Heap-allocates a payload structure to pack function arguments together safely across thread boundaries.
* **Line 5 (`pthread_create`):** Invokes POSIX thread creation, passing the target function wrapper and the packed heap argument structure.
* **Lines 6–7 (`pthread_join`):** Retrieves the raw 64-bit thread handle and blocks execution until the target worker thread finishes.

---

## Task 2: The AST Optimizer

When passed `--optimize`, the compiler executes three AST-level transformations before IR generation:
1. **Unused Variables Removal (`var_remover`):** Removes unused declarations and non-side-effect assignments.
2. **Unused Functions Removal (`fn_remover`):** Prunes unreferenced top-level functions.
3. **Function Inlining (`inliner`):** Replaces single-call non-recursive functions directly at call sites.

### Inlining IR Excerpt (Before vs. After)

**Before Optimization**
```llvm
define i32 @compute(i32 %x) {
entry:
  %add = add i32 %x, 5
  ret i32 %add
}

define i32 @main() {
entry:
  %call = call i32 @compute(i32 10)
  ret i32 %call
}
```

**After Optimization (`--optimize`)**
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

---

## Task 3: Interactive Program (`programs/teacup.txt`)

* **Program Description:** Renders an interactive ASCII animation loop of a spinning teacup. It reads user preferences for the number of full turns (or 0 for infinite animation) and rotation speed, then calculates and displays each frame dynamically.
* **Feature Usage:** Uses `потік`, `витечи`, and `дочекатись` inside the animation loop to spawn `frame_worker` background threads for frame timing synchronization concurrently alongside frame rendering. Uses `для` loops, `clear()` screen refreshes, and `допоки (повне)` for infinite mode.
* **Running Recorded Sessions:**

```bash
# 1. Build native executable
./build/compiler --optimize programs/teacup.txt build/teacup.ll
llc -relocation-model=pic -filetype=obj build/teacup.ll -o build/teacup.o
clang build/teacup.o -pthread -o build/teacup

# 2. Run Session 1 (2 Turns, Fast Speed)
./build/teacup < programs/teacup.in

# 3. Run Session 2 (infinite, Slow Speed)
./build/teacup < programs/teacup_1turn.in
```
