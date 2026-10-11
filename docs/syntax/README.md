# WOS Syntax Reference

## Variables

WOS supports these types:

| Type | Description |
|------|-------------|
| `int` | Integer number |
| `float` | Floating-point number |
| `double` | Double precision float |
| `bool` | Boolean (true/false) |
| `string` | Text string |
| `void` | No type |

### Declaration

```wos
int x = 10;
string name = "WOS";
float pi = 3.14;
bool flag = true;
```

Operators

Arithmetic

· + — Add
· - — Subtract
· * — Multiply
· / — Divide
· % — Modulo

Comparison

· > — Greater than
· < — Less than
· >= — Greater or equal
· <= — Less or equal
· == — Equal
· != — Not equal

Conditionals

if

```wos
if x > 5 {
    print(x);
}
```

if / else

```wos
if x > 5 {
    print(x);
} else {
    print(0);
}
```

Loops

while

```wos
int i = 0;
while i < 5 {
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

Functions

Declaration

```wos
function add(int a, int b) {
    return a + b;
}
```

Call

```wos
int result = add(5, 3);
print(result);  // 8
```

Built-in Functions

print

```wos
print("Hello");     // prints text
print(42);          // prints number
print(x);           // prints variable
```

WosGUI (Hidden Feature)

WOS includes a built-in GUI library called WosGUI.
It compiles to C with raygui.

window

```wos
window "My App" 800 x 600;
```

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

· center
· top
· bottom
· left
· right

Shape keywords

· rectangle
· circle
· triangle

Complete Example

```wos
int x = 10;

function add(int a, int b) {
    return a + b;
}

if x > 5 {
    int result = add(x, 5);
    print(result);
}

while x < 15 {
    print(x);
    x = x + 1;
}

window "WOS App" 800 x 600;
text "Welcome" top;
button rectangle "Click" center 100 x 40;
```

