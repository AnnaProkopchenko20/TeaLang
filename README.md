# Tea language compiler: Stage 1 (lexer and parser)

A small statically-typed language with Ukrainian keywords and a tea theme.
Stage 1 contains a hand-written, byte-level, state-machine lexer and a
hand-written recursive-descent parser that builds an AST and dumps it as an
indented tree. No regular expressions, `split()`, lexer generators or parser
generators are used.

Related files: [`grammar.ebnf`](grammar.ebnf) (full grammar),
[`ai_usage.txt`](ai_usage.txt) (AI disclosure), `tests/` (test suite),
`run.sh` (test runner).

---

## Language at a glance

| Concept | Keyword / symbol | Meaning |
|---|---|---|
| 32-bit integer | `горнятко_32чл` | `i32` |
| 64-bit integer | `кухоль_64чл` | `i64` |
| boolean | `заповненість` | `bool` |
| mutable marker | `відкрита` | variable may be reassigned |
| true / false | `повне` / `пусте` | boolean literals |
| if / else | `якщо` / `інакше` | conditional |
| loop | `допоки` | `while` loop |
| exit | `чайна_перерва` | stop the program with a value |
| declaration | `:=` | `type name := value` |
| assignment | `=>` | `value => name` (value on the left) |
| comparison | `==`, `!=` | equality, inequality |
| arithmetic | `+`, `-`, `*` | integer arithmetic |
| negation | `!` | boolean not |

Source files are UTF-8. Identifiers may contain Latin and Cyrillic letters,
digits and `_` (for example `кількість_чаю`).

### Unusual syntax features

* **Reversed assignment.** The value comes first, the target last:
  `x + 1 => x`.
* **Line-oriented layout.** Every statement ends at the end of its line.
  There is no `;`.
* **Braces on their own line.** `{` and `}` must each be alone on a line,
  and so must `інакше`. `якщо (c) {` on one line is a syntax error.
* **Cyrillic multi-byte keywords.** Keywords are UTF-8 byte sequences
  recognised by the same state machine as ASCII words. Columns in error
  messages count characters, not bytes.

### 1. Integer types

```text
горнятко_32чл small := 42
кухоль_64чл big := 9000000000
```

### 2. Boolean type

```text
заповненість ready := повне
заповненість empty := пусте
```

### 3. Constants and variables

There are two kinds of binding:

* `type name := value` is **immutable**. It is a constant: it cannot be
  reassigned. Immutable bindings are the language's only way to declare a
  constant, so "constants" and "constant variables" are the same construct.
* `відкрита type name := value` is **mutable**.

```text
горнятко_32чл limit := 100
відкрита горнятко_32чл counter := 0
```

The AST dump shows the flag as `const` or `відкрита`.

### 4. Assignment

```text
counter + 1 => counter
```

### 5. Comparison and operators

`==` and `!=` produce a `bool`. They are not chainable (`a == b == c` is a
syntax error). Precedence from loosest to tightest:
`==` `!=`, then `+` `-`, then `*`, then prefix `!`. `+ - *` are left
associative. Expressions have no parentheses; `(` `)` appear only around the
condition of `якщо` and `допоки`.

```text
заповненість same := 1 + 2 == 3
заповненість flipped := !повне
```

### 6. Conditional (`якщо` / `інакше`)

The `інакше` part is optional. Each block must contain at least one
statement.

```text
якщо (counter == 100)
{
    чайна_перерва 1
}
інакше
{
    counter + 1 => counter
}
```

### 7. Loop (`допоки`)

```text
відкрита горнятко_32чл i := 0
допоки (i != 3)
{
    i + 1 => i
}
```

### 8. Exit

`чайна_перерва` takes a single factor (number, boolean, variable, or `!` of
one):

```text
чайна_перерва 0
```

### Complete example

```text
відкрита горнятко_32чл лічильник := 10
заповненість є_готовим := повне
якщо (лічильник == 10)
{
    лічильник + 1 => лічильник
}
чайна_перерва 0
```

---

## Scope of Stage 1 and what is deliberately deferred

Stage 1 is **syntax only**. Everything below is a *semantic* property and is
therefore planned for Stage 2, not checked here:

| Requirement | Status in Stage 1 |
|---|---|
| Type overflow checks | **Not checked.** Numbers are parsed as digit strings with no range limit, so `горнятко_32чл x := 2147483648` parses successfully. Range checking against `i32` / `i64` (and checks on arithmetic results) belongs to the semantic stage, where the declared type is known. The tests `overflow_i32_literal_parses` and `overflow_i64_literal_parses` record this behaviour on purpose. |
| Assigning to an immutable binding | Not checked (parses fine). Stage 2. |
| Use of undeclared / redeclared variables | Not checked. Stage 2. |
| Type checking (for example `заповненість x := 1 + 2`, or `!` on an integer) | Not checked. Stage 2. |

What **is** checked in Stage 1: all lexical errors (unknown byte) and all
syntax errors described by `grammar.ebnf`, such as unclosed or empty blocks,
missing `(`/`)`/`{`/`:=`/`=>`, stray `інакше` or `}`, missing operands, and
tokens left over at the end of a statement.

Also not in the language: unary minus (no negative literals), parenthesised
sub-expressions, comparison operators other than `==` / `!=`, comments, and
string or floating-point literals.

---

## Requirements

* C++17 compiler (GCC or Clang)
* CMake 3.20+
* LLVM development headers and libraries. `visitor.h` includes
  `<llvm/IR/Value.h>`, because the AST visitor interface is shared with the
  code-generation stage. Stage 1 itself does not generate any code.
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
./build/compiler -ast path/to/program.txt
```

(`--ast` is accepted as well. `-tokens` / `--tokens` prints the token stream
instead and is meant for debugging.)

* **Success:** the AST is printed to `stdout`, indented two spaces per depth,
  exit code 0.
* **Error:** nothing on `stdout`, exactly one line on `stderr`, exit code 1:

```text
compilation error: line L:C: message
```

Only the first error is reported. `L` is the line and `C` the column, both
starting at 1, with columns counted in characters.

### AST dump format

```text
Program
  Decl лічильник горнятко_32чл відкрита      Decl <name> <type> <відкрита|const>
    Const 10                                 number literal
  якщо                                       if: children are condition, Block,
    BinOp ==                                   then optionally "інакше" + Block
      Var лічильник
      Const 10
    Block
      Assign лічильник                       Assign <target>; child is the value
        BinOp +
          Var лічильник
          Const 1
  чайна_перерва
    Not !                                    negation
      Bool пусте                             Bool повне | пусте
```

Other nodes: `допоки` (children: condition, `Block`). `інакше` appears as a
child of `якщо` and holds the else `Block`.

## Tests

```text
tests/ok/    valid programs:    name.txt  +  name.ast   (expected stdout)
tests/err/   invalid programs:  name.txt  +  name.err   (expected stderr line)
```

* 28 happy-path tests and 27 error tests (each feature is covered by a program
  that uses it and one that misuses it, where misuse is a syntactic error).
* The runner builds the compiler, runs every test and prints a diff for each
  failure. For error tests it also checks that the exit code is non-zero and
  that nothing is printed to `stdout`.

```bash
chmod +x run.sh
./run.sh
```

The script exits with status 0 only if all tests pass and prints
`passed: N   failed: M` at the end.

Semantic behaviour (overflow, mutability, typing) is not covered by these
tests because it is not part of Stage 1. It will be covered in Stage 2.

## Project layout

```text
grammar.ebnf        complete grammar (syntax and lexical rules)
ai_usage.txt        AI usage disclosure
compiler.cpp        command-line driver
lexer.h / lexer.cpp           lexer entry point (state-machine driver)
lexer_context.h, states/      lexer context and the individual states
parser.h / parser.cpp         recursive-descent parser, one function per rule
ast.h, ast_fwd.h, nodes/      AST node class hierarchy (every node keeps line:col)
visitor.h           visitor interface (used from Stage 2)
keywords.h          the only place where keywords are spelled
tests/              test programs and expected outputs
run.sh              test runner
```
