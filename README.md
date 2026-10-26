# Tea language compiler: Stage 2 (code generation, functions, structures)

A small statically-typed language with Ukrainian keywords and a tea theme.
The compiler has a hand-written, byte-level, state-machine lexer, a
hand-written recursive-descent parser that builds an AST, a separate semantic
pass, and a code generator that builds LLVM IR through `llvm::IRBuilder`.
No regular expressions, `split()`, generators or string-assembled IR are used.

Related files: [`grammar.ebnf`](grammar.ebnf) (full grammar),
[`ai_usage_2.txt`](ai_usage_2.txt) (AI disclosure), `tests/` (test suite),
`run.sh` (test runner).

---

## Language at a glance

| Concept | Keyword / symbol | Meaning |
|---|---|---|
| 32-bit integer | `горнятко_32чл` | `i32` |
| 64-bit integer | `кухоль_64чл` | `i64` |
| boolean | `заповненість` | `bool` |
| mutable marker | `відкрита` | variable or field may be changed |
| global constant | `факт` | constant declared before everything else |
| true / false | `повне` / `пусте` | boolean literals |
| if / else | `якщо` / `інакше` | conditional |
| loop | `допоки` | `while` loop |
| exit | `чайна_перерва` | leave the program or the function with a value |
| declaration | `:=` | `type name := value` |
| assignment | `=>` | `value => target` (value on the left) |
| structure | `struct` | named type with fields and member functions |
| function | `fn`, `->` | `fn name := (params) -> type` |
| field access | `.` | `object.field`, `object.function()` |
| input / output | `read()`, `print(...)` | built-ins |
| comment | `#` | to the end of the line |
| comparison | `==`, `!=` | equality, inequality |
| arithmetic | `+`, `-`, `*` | integer arithmetic |
| negation | `!` | boolean not |

Source files are UTF-8. Identifiers may contain Latin and Cyrillic letters,
digits and `_` (for example `кількість_чаю`).

### Unusual syntax features

* **Reversed assignment.** The value comes first, the target last:
  `x + 1 => x`, `5 => c.center.x`.
* **Line-oriented layout.** Every statement ends at the end of its line.
  There is no `;`.
* **Braces on their own line.** The `{` and `}` of a block (`якщо`, `допоки`,
  a structure, a function) must each be alone on a line, and so must `інакше`.
  The braces of an object value, `{3, 4}`, are written inline.
* **Cyrillic multi-byte keywords.** Keywords are UTF-8 byte sequences
  recognised by the same state machine as ASCII words. Columns in error
  messages count characters, not bytes.
* **No parentheses in expressions.** `(` `)` appear only around the condition
  of `якщо` / `допоки` and in calls.

---

## Features, one short example each

### Integer and boolean types

```text
горнятко_32чл small := 42
кухоль_64чл big := 9000000000
заповненість ready := повне
```

### Constants and variables

`type name := value` is a constant, `відкрита type name := value` is a
mutable variable. An immutable binding cannot be assigned again.

```text
горнятко_32чл limit := 100
відкрита горнятко_32чл counter := 0
counter + 1 => counter
```

### Comparison, operators, `!`

`==` and `!=` give a `bool` and are not chainable. Precedence, loosest to
tightest: `==` `!=`, then `+` `-`, then `*`, then prefix `!`.

```text
заповненість same := 1 + 2 == 3
заповненість flipped := !повне
```

### Conditional and loop

```text
якщо (counter == 100)
{
    чайна_перерва 1
}
інакше
{
    counter + 1 => counter
}
допоки (counter != 3)
{
    counter + 1 => counter
}
```

### Exit

`чайна_перерва` takes one factor (number, boolean, variable, field chain,
call, or `!` of one). At the top level it ends the program, which prints
`Program exit with result N` (`true` / `false` for a bool). In a function it
ends the function and its operand is the result. It may appear anywhere in a
body or block, but the last statement of every body must be one.

```text
чайна_перерва 0
```

### Comments

A comment starts at `#` and runs to the end of the line. It may follow any
token or stand on its own line. `#` inside a string literal is an ordinary
character.

```text
горнятко_32чл змінна := 32 # a comment
# a line of its own
```

### Global constants

```text
факт горнятко_32чл MAX := 100
факт горнятко_32чл DOUBLE := MAX * 2
факт заповненість DEBUG := пусте
```

### Functions

```text
fn додати := (горнятко_32чл a, горнятко_32чл b) -> горнятко_32чл
{
    горнятко_32чл s := a + b
    чайна_перерва s
}
горнятко_32чл r := додати(2, 3)
чайна_перерва r
```

A call is an expression and may be an argument of another call. A call alone
is not a statement: write `f(x) => r`.

### Structures and objects

```text
struct Point
{
    горнятко_32чл відкрита x
    горнятко_32чл відкрита y
}
struct Circle
{
    Point center
    горнятко_32чл radius
}
відкрита Point p := {3, 4}
Circle c := {p, 2}
10 => p.x
горнятко_32чл cx := c.center.x
```

### Member functions

```text
struct Point
{
    горнятко_32чл відкрита x
    горнятко_32чл відкрита y
    fn d2 := () -> горнятко_32чл
    {
        горнятко_32чл r := x * x + y * y
        чайна_перерва r
    }
    fn shift := (горнятко_32чл dx) -> горнятко_32чл
    {
        x + dx => x
        чайна_перерва x
    }
}
відкрита Point p := {3, 4}
горнятко_32чл d := p.d2()
горнятко_32чл n := p.shift(2)
```

### `read` and `print`

```text
горнятко_32чл a := read()
print(a)
print("привіт")
print(a == 3)
print(p)
```

---

## Rules enforced by the semantic pass

Each rule is covered by one program that follows it and one that breaks it
(`tests/`, files named after the rule). Every error is one line
`compilation error: line L:C: message`.

### Program structure

* **P1 Order.** A program is: constants, then structures, then functions, then
  statements. A constant after a structure, a function or a statement, or a
  structure after a function, is a syntax error at that declaration.
* **P2 Final exit.** The last statement of the program body, of every function
  body and of every member function body is a `чайна_перерва`. Otherwise it is
  a syntax error at the closing `}` or at the end of the file. Earlier `exit`
  statements are allowed anywhere.
* **P3 Unreachable code (warning).** A statement that follows a
  `чайна_перерва` in the same body or block can never run. The compiler
  prints `warning: line L:C: unreachable code` for the first such statement of
  that body or block, still checks its names and types, and still compiles.
  An `якщо`/`інакше` whose two branches both end in `чайна_перерва` does not
  make the code after it unreachable for this warning. The warning goes to
  stderr, the exit code is 0, and warnings are not printed when the compilation
  fails with an error.

### Constants

* **C1 Form.** A constant has a built-in type, is always immutable and is
  initialised by a constant expression: literals, earlier constants, and the
  operators `+ - * == != !`. No `read`, calls, fields or objects. `факт`
  with `відкрита` or with a structure type is a syntax error.
* **C2 Compile-time value.** The initialiser is evaluated by the compiler. Its
  type must fit the declared type (an `i32` value may initialise an `i64`
  constant, not the reverse), and a literal or an intermediate result that
  does not fit its type is an overflow error. Using a constant declared
  below or in its own initialiser is "used before its declaration".
* **C3 Visibility.** A constant is visible in the program body, in every
  function and in every member function. It cannot be assigned.
* **C4 Shadowing.** A parameter, a local variable or a field may have the
  name of a constant; the inner name wins.
* **C5 Distinct names.** Constants, structures and (top-level) functions share
  one namespace: a repeated name is an error at the second declaration.

### Names and scopes

* **N1 Declaration before use; one declaration per block.** A name must be
  declared before it is used. Declaring a name twice in the same block is an
  error; an inner block may declare the same name again, with any type
  (shadowing). A name declared in a block is gone after it.
* **N2 What a function body sees.** A function body sees its parameters, its
  own variables and the global constants. It does not see the variables of the
  program body. It sees all functions, also those declared below it, and itself.
* **N3 Lookup inside a member function.** A name is looked up among the
  parameters and local variables (innermost block first), then among the
  fields of the structure, then among the global constants. A parameter or a
  local named like a field hides the field; a field named like a constant
  hides the constant.
* **N4 Calls inside a member function.** `f(...)` looks first among the member
  functions of the same structure, then among the global functions. A member
  function hides a global function with the same name.
* **N5 Parameters are copies** and cannot be assigned. Structure arguments and
  results are copied too.
* **N6 No names of types or functions.** A parameter, a local variable
  (also at the top level) or a field may not have the name of a structure or
  of a top-level function, so `f(...)` can never be read as both a call and a
  use of a variable. Inside a member function, a parameter or a local variable
  may not have the name of a member function of the same structure either (a
  field already may not, see S1). The error is reported at the declaration,
  whichever of the two is declared first. Global constants are different: they
  may be shadowed (C4).

### Types and calls

* **T1 Types.** Arithmetic needs integers, `if`/`while` conditions and `!`
  need `bool`, `==` and `!=` need operands of the same type. An `i32` value
  may be used where an `i64` is expected, not the reverse.
* **F1 Calls.** The number of arguments equals the number of parameters and
  each argument type fits its parameter. The call has the result type of the
  function.
* **F2 Result.** The operand of `чайна_перерва` in a function has the result
  type of the function. At the top level it has a built-in type.
* **F3 Reserved.** `read` and `print` are keywords: they cannot name a
  variable, a function, a parameter, a field or a member function.
  `read(5)` and `print()` are syntax errors; `print` has no value.

### Structures

* **S1 Declaration.** A structure has at least one field. A field type is
  built-in or a structure declared above (a structure cannot contain itself).
  Fields come first, then member functions; a field after a member function,
  and a member function before the first field, are errors. A field and a
  member function, or two member functions, may not share a name.
* **S2 Objects.** An object is created with one value per field, in order,
  each of the field's type; an inner `{...}` is allowed only for a field that
  is a structure. `Point q := p` copies an object.
* **S3 Fields.** Every link of `a.b.c` must exist. A field is reached by its
  index in the structure, not by name, in the generated code.
* **S4 Assignment through a chain.** `value => a.b.c` needs every link to be
  mutable: the object (`відкрита`) and each field on the way (`відкрита`).
  A constant object or a constant field cannot be assigned, also inside a
  member function.

### Member functions

* **M1 Calls.** A member function is called on an object (a variable or a
  field chain, `c.center.d2()`), never on a type or on an `i32`. The chain ends
  at the call. The object is passed by address.
* **M2 Mutating.** A member function is mutating if it assigns a field of its
  object, or calls a mutating member function on its object or on one of its
  fields. A local variable named like a field does not make it mutating. A
  mutating function may be called only through a chain in which the object and
  every field are `відкрита`; a non-mutating one may be called on anything.

### Built-ins

* **B1 `read()`** reads one `i32` from standard input. It stands wherever a
  factor can.
* **B2 `print`** is a statement and writes its argument followed by a newline.
  An integer is printed as a number, a bool as `true` or `false`, a string
  literal as written (no escape sequences, it stands only inside `print`), and
  an object as its type name and fields in declaration order, an inner object
  the same way: `Circle{center: Point{x: 3, y: 4}, radius: 2}`. Output of
  `print` comes before the final `Program exit with result N` line.

### Not in the language

Unary minus, parentheses in expressions, comparison operators other than
`==` / `!=`, string variables, floating-point numbers, global variables that
can change, and a `.` after a call (`f().x`).

---

## Requirements

* C++17 compiler (GCC or Clang)
* CMake 3.20+
* LLVM development headers and libraries
* To run the generated programs: `lli`, or `clang` (and `llc`)
* A UTF-8 terminal and UTF-8 source files

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --target compiler
cd ..
```

This produces `build/compiler`.

## Run

```bash
./build/compiler input.txt output.ll      # writes the LLVM IR
./build/compiler --ast input.txt          # prints the AST
lli output.ll                             # run the program
clang output.ll -o program && ./program   # or compile and run natively
```

(`-ast` is accepted as well as `--ast`.)

* **Success:** `output.ll` is written. Warnings, if any, are printed to
  `stderr` as `warning: line L:C: message`; the exit code is 0.
* **Error** (lexical, syntax or semantic): exactly one line on `stderr`,
  exit code 1, no output file, no warnings:

```text
compilation error: line L:C: message
```

Only the first error is reported. `L` is the line and `C` the column, both
starting at 1, with columns counted in characters.

### AST dump

`--ast` prints one node per line, indented two spaces per depth, with the
fields and flags of each node: the type and `const` / `відкрита` flag of a
declaration, the parameter and result types of a function, the mutability of a
field, the arguments of a call, and whether a chain ends in a member call.
Global constants are `GlobalDecl` nodes at the start of the program.

## Tests

```text
tests/ok/    valid programs, AST:   name.txt + name.ast
tests/e2e/   valid programs, run:   name.txt + name.expected [+ name.in] [+ name.warn]
tests/err/   invalid programs:      name.txt + name.err      (expected stderr line)
```

* `ok`: the AST must equal `name.ast`.
* `e2e`: the program is compiled to `.ll`, built and run with `name.in` as
  standard input (when it exists), and its whole output is compared with
  `name.expected`. When `name.warn` exists, the compiler's stderr must equal it.
* `err`: the exit code is non-zero, nothing is written to stdout, and stderr
  equals `name.err`.

```bash
chmod +x run.sh
./run.sh
```

The script runs every test, prints the names of the failed ones and exits with
status 0 only if all pass.

### Backward compatibility and updated/deleted Stage 1 tests

Due to grammar and language changes that intentionally broke backward compatibility with Stage 1:
* **Final exit required (Rule P2):** Every body must end with an exit statement (`чайна_перерва`). Stage 1 tests in `tests/ok/` that did not have an exit statement were updated by appending `чайна_перерва 0` to preserve the feature under test while satisfying the new rule.
* **`tests/ok/empty_program` (deleted):** An empty file contains no statements and thus has no exit statement, which is now a syntax error under Rule P2. It was intentionally deleted.
* **`tests/err/lex_invalid_char_hash_in_if` (deleted):** The `#` character is now recognized as a comment to the end of the line instead of an invalid byte; this test was intentionally deleted.

## Project layout

```text
grammar.ebnf        complete grammar (syntax and lexical rules)
ai_usage_2.txt      AI usage disclosure
compiler.cpp        command-line driver
lexer.h / lexer.cpp           lexer entry point (state-machine driver)
lexer_context.h, states/      lexer context and the individual states
parser.h / parser.cpp         recursive-descent parser, one function per rule
ast.h, ast_fwd.h, nodes/      AST node class hierarchy (every node keeps line:col)
visitor.h                     visitor interface
semantic_checker.h / .cpp     symbol table and semantic pass
codegen.h / codegen.cpp       LLVM IR generation through IRBuilder
keywords.h                    the only place where keywords are spelled
tests/                        test programs and expected outputs
run.sh                        test runner
```
