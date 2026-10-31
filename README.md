# my_preprocessor

`my_preprocessor` is a simple **C preprocessor** written in C.

It takes a C source file (`file.c`) and creates an expanded source file (`file.i`). To do this it:

1. removes comments
2. includes header files
3. substitutes macros

---

## 1. Features

### Comment removal
* Removes single-line comments: `// ...`
* Removes multi-line comments: `/* ... */`
* Leaves comment symbols inside strings alone, so `"http://abc"` stays as it is.

### File inclusion
* `#include "file.h"`: the contents of `file.h` are copied in place of this line.
* A header can include another header (nested include).
* Header guards (`#ifndef` / `#define` / `#endif`) work, so a header included twice is copied only once.
* `#include <stdio.h>`: system headers are left unchanged.

### Macro substitution
| Type | Example | Result |
|------|---------|--------|
| Simple macro | `#define MAX 100` → `MAX` | `100` |
| Macro with arguments | `#define SQUARE(x) ((x)*(x))` → `SQUARE(5)` | `((5)*(5))` |
| Nested macro | `SQUARE(SQUARE(2))` | `((((2)*(2)))*(((2)*(2))))` |
| Stringify `#` | `#define STR(x) #x` → `STR(hello)` | `"hello"` |
| Token join `##` | `#define CAT(a,b) a##b` → `CAT(var,1)` | `var1` |
| Multi-line macro | `#define ADD(a,b) \`<br>`((a)+(b))` | works |
| Undefine | `#undef MAX` | `MAX` is no longer replaced |

### Conditional compilation
`#ifdef`, `#ifndef`, `#if`, `#else`, `#endif`

---

## 2. Project files

| File | What it is |
|------|------------|
| `my_preprocessor.c` | The preprocessor program (all the code) |
| `my.c` | Sample input C program |
| `my.h` | Sample header file, included by `my.c` |
| `my.i` | Output created by running `./my_preprocessor my.c` |

All the code is in **`my_preprocessor.c`**:

| Part | Functions | Work |
|------|-----------|------|
| Step 1 | `remove_comments()` | Removes comments |
| Step 2 | `process_file()`, `do_include()`, `handle_directive()` | Reads the file line by line and handles `#include` and the other directives |
| Step 3 | `add_macro()`, `expand_macros()`, `substitute()` | Stores macros and replaces them in the code |
| main | `main()` | Checks the input file and creates the `.i` output file |

---

## 3. Compile

```sh
gcc my_preprocessor.c -o my_preprocessor
```

## 4. Run

```sh
./my_preprocessor my.c
```

Output:
```
Done: my.c -> my.i
```

The new file `my.i` is created in the same folder as `my.c`.

To check that the output is correct C code:
```sh
gcc -x c my.i -o my
./my
```
Expected:
```
12.560000
100
5
```

---

## 5. Example

**Input: `my.h`**
```c
#ifndef MY_H
#define MY_H

#define MAX 100
int add(int a, int b);

#endif
```

**Input: `my.c`**
```c
#include <stdio.h>
#include "my.h"
#define PI 3.14
#define SQUARE(x) ((x) * (x))

/* area of circle */
int add(int a, int b) { return a + b; }

int main(void)
{
    int n = MAX;                       // from my.h
    printf("%f\n", PI * SQUARE(2));
    printf("%d\n", n);
    printf("%d\n", add(2, 3));
    return 0;
}
```

**Output: `my.i`**
```c
#include <stdio.h>

int add(int a, int b);

 
int add(int a, int b) { return a + b; }

int main(void)
{
    int n = 100;
    printf("%f\n", 3.14 * ((2) * (2)));
    printf("%d\n", n);
    printf("%d\n", add(2, 3));
    return 0;
}
```

What changed:
* The comments are removed.
* The code from `my.h` is copied in place of `#include "my.h"`.
* `MAX`, `PI` and `SQUARE(2)` are replaced with their values.
* The `#define` lines are gone.

---

## 6. How it works

```
        file.c
          │
          ▼
   ┌──────────────┐
   │  read file   │
   └──────┬───────┘
          ▼
   ┌──────────────┐
   │   remove     │   Step 1
   │   comments   │
   └──────┬───────┘
          ▼
   read line by line
          │
    ┌─────┴───────────────────────┐
    ▼                             ▼
 line starts with #           normal line
    │                             │
    ├─ #include → open that       ▼
    │   file, repeat steps   replace macros   Step 3
    │   (Step 2)                  │
    ├─ #define → save macro       │
    ├─ #undef  → delete macro     │
    └─ #ifdef ... → keep/skip     │
                                  ▼
                               file.i
```

**Step 1: Comment removal.** The program reads the file character by character.
* On `"` or `'`, it copies the whole string as it is.
* On `//`, it skips to the end of the line.
* On `/*`, it skips to `*/`.

**Step 2: File inclusion.** On `#include "x.h"`, the function `process_file()` calls itself for `x.h` (recursion). So the header's code is processed and written at that place.

**Step 3: Macro substitution.**
1. Each `#define` is saved in an array of structures (name, parameters, body).
2. For every word in a normal line, the program checks whether it is a macro.
   * If it is a simple macro, the word is replaced by the macro's body.
   * If it takes arguments, the arguments are read from `( )` and put in place of the parameters.
3. The result is checked again for more macros, so `AREA(r)` → `PI * SQUARE(r)` → `3.14 * ((r)*(r))`.
4. A macro that is being expanded is marked busy, so `#define X X+1` does not loop forever.

---

## 7. Limitations

* `#elif` is not supported.
* `#if` works only with a number or a macro (`#if 0`, `#if DEBUG`).
* System headers like `<stdio.h>` are not expanded.

---

## 8. Hardware / circuit details

Not applicable. This is a software-only project.

---

## Author

**Manjulatha Pathakuntla**
