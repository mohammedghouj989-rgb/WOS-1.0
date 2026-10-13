```markdown
# WOS Syntax Reference

## Table of Contents
1. [Comments](#comments)
2. [Types](#types)
3. [Variables](#variables)
4. [Operators](#operators)
5. [Control Flow](#control-flow)
6. [Functions](#functions)
7. [Structs](#structs)
8. [Lists (Generic)](#lists-generic)
9. [Memory Management](#memory-management)
10. [Imports](#imports)
11. [Library Mode](#library-mode)
12. [WosGUI](#wosgui)

---

## Comments

```wos
// This is a single-line comment

/* This is a
   multi-line comment */
```

---

Types

Type Description
int Integer number
float Floating-point number
double Double precision float
bool Boolean (true / false)
string Text string
void No type
void* Generic pointer
list<T> Dynamic array of type T

---

Variables

```wos
int x = 10;
float pi = 3.14;
double e = 2.718;
bool flag = true;
string name = "WOS";
```

Variables must be declared with an explicit type. No let or var.

---

Operators

Arithmetic

Op Meaning
+ Add
- Subtract
* Multiply
/ Divide
% Modulo

Comparison

Op Meaning
> Greater than
< Less than
>= Greater or equal
<= Less or equal
== Equal
!= Not equal

Logical

Op Meaning
&& AND
\|\| OR
! NOT

Memory

Op Meaning
*ptr Dereference
&x Address-of
*ptr = v Assign through pointer

---

Control Flow

if / else if / else

```wos
if x > 5 {
    print("big");
} else if x > 0 {
    print("small");
} else {
    print("negative");
}
```

while

```wos
int i = 0;
while i < 10 {
    print(i);
    i = i + 1;
}
```

for

```wos
for (int i = 0; i < 10; i = i + 1) {
    print(i);
}
```

break / continue

```wos
while true {
    if x > 100 { break; }
    if x == 5 { continue; }
    x = x + 1;
}
```

---

Functions

Declaration

```wos
function add(int a, int b) {
    return a + b;
}
```

· Functions can only be declared at the top level.
· Parameters have explicit types.
· Return type is always int (unless a return type is specified with ->).

Calling

```wos
int result = add(5, 7);
print(result);   // 12
```

With explicit return type

```wos
function make_point(int x, int y) -> Point {
    return Point(x, y);
}
```

---

Structs

Definition

```wos
struct Point {
    int x;
    int y;
}
```

· Structs can only be defined at the top level.
· Fields have explicit types.

Creating

```wos
Point p = Point(50, 60);
```

The order of arguments must match the order of fields.

Accessing fields

```wos
print(p.x);      // 50
print(p.y);      // 60

p.x = 100;       // modify
```

Pointers to structs

```wos
Point* ptr = &p;
print(ptr->x);   // 50
```

---

Lists (Generic)

WOS lists are dynamic arrays. They can hold any type:
int, float, double, bool, string, or even structs.

Integer list

```wos
list<int> numbers = [10, 20, 30];
print(numbers[0]);   // 10
print(numbers[1]);   // 20

numbers[1] = 99;
print(numbers[1]);   // 99
```

String list

```wos
list<string> names = ["Ali", "Sara", "Omar"];
print(names[0]);   // Ali
print(names[1]);   // Sara
```

Float list

```wos
list<float> prices = [1.5, 2.5, 3.5];
print(prices[0]);   // 1
```

Struct list

```wos
list<Point> points = [Point(1, 2), Point(3, 4)];
print(points[0].x);   // 1
```

Default: list = list<int>

```wos
list numbers = [10, 20, 30];   // same as list<int>
```

Empty list

```wos
list<int> empty;
```

An empty list gets capacity 4 by default and can grow.

---

Memory Management

WOS uses manual memory management (like C).
There is no garbage collector.

alloc

Allocates size bytes on the heap and returns a void*.

```wos
void* ptr = alloc(100);
```

deref

Read or write the value pointed to by a pointer:

```wos
*ptr = 42;
print(*ptr);   // 42
```

address-of

Gets the address of a variable:

```wos
int x = 10;
void* addr = &x;
```

free

Frees the memory allocated by alloc:

```wos
free(ptr);
```

Full example

```wos
void* ptr = alloc(sizeof(int));
*ptr = 42;
print(*ptr);      // 42
free(ptr);
```

Notes

· WOS does not check for memory leaks at compile time.
· A separate tool (woscheck, coming soon) will do that.
· If you forget free, you leak memory — just like in C.

---

Imports

Include a header from C or another WOS library:

```wos
import "stdio.h";
import "mymath.h";
```

Compiles to:

```c
#include "stdio.h"
#include "mymath.h"
```

---

Library Mode

WOS supports two modes:

1. Program mode — writes output.c, compiles it, and runs it.
2. Library mode — writes output.c (no main) plus a .h file.

How to declare a library

Put this at the very top of your file:

```wos
library "mymath";

function square(int x) {
    return x * x;
}

function cube(int x) {
    return x * x * x;
}
```

What gets generated

```bash
./wos mymath.wos
```

Writes:

· output.c — the library source (no main)
· mymath.h — the header file with function prototypes

Generated header (mymath.h)

```c
#ifndef MYMATH_H
#define MYMATH_H

int square(int x);
int cube(int x);

#endif
```

Using the library

In another WOS file:

```wos
import "mymath.h";

int r = square(5);
print(r);   // 25
```

Or from plain C:

```c
#include "mymath.h"
int main(void) {
    return square(5);
}
```

Compiling a library to a static archive

WOS can also build the library object file:

```bash
clang -std=c11 -c output.c -o mymath.o
ar rcs libmymath.a mymath.o
```

---

WosGUI

WOS has a built-in GUI library called WosGUI.
It compiles to C with raygui.

window

```wos
window "My App" 800 x 600;
```

If omitted, defaults to 800 x 600.

text

```wos
text "Hello" top;
text "Middle" center;
text "Bottom" bottom;
```

button

```wos
button rectangle "Click Me" center;
button circle "Go" left 100 x 40;
button triangle "Back" right;
```

input

```wos
input "Enter name" center;
```

Position keywords

· center (default)
· top
· bottom
· left
· right

Shape keywords

· rectangle (default)
· circle
· triangle

Full example

```wos
window "WOS App" 800 x 600;

text "Welcome" top;

button rectangle "Click Me" center 120 x 40;

input "Enter your name" bottom;
```

Building

WosGUI requires raygui. On Termux:

```bash
pkg install raylib
```

Then compile with:

```bash
./wos gui_example.wos
```

---

Complete Example

```wos
library "mylib";

import "stdio.h";

struct Point {
    int x;
    int y;
}

function add(int a, int b) {
    return a + b;
}

function main_demo() {
    int x = 10;

    if x > 5 {
        print("big");
    }

    list<int> nums = [1, 2, 3];
    print(nums[0]);

    list<string> names = ["Ali", "Sara"];
    print(names[0]);

    Point p = Point(50, 60);
    print(p.x);

    void* ptr = alloc(100);
    *ptr = 42;
    print(*ptr);
    free(ptr);

    int r = add(5, 7);
    print(r);
}
```

---

Summary of New Features

Feature Syntax Notes
Generic list list<int> x = [1, 2, 3]; Holds any type
String list list<string> s = ["a", "b"]; Uses %s in print
Alloc void* p = alloc(100); Like C's malloc
Free free(p); Like C's free
Dereference *p = 42; Read/write through pointer
Address-of &x Get pointer to x
Struct init Point p = Point(1, 2); Positional arguments
Library mode library "name"; Generates .h file
Pointer to struct Point* p = &pt; p->x Arrow access

``
