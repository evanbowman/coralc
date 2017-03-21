# coralc

## Introduction

Note: I am no longer working on this project. My goal here was to better understand how compilers work. Feel free to take the code and do whatever with it.

coralc is the compiler for a new programming language, called Coral. Coral has a simple syntax inspired by Ruby and Lua, but compiles to native machine code.

## Language features

### Type inferencing
No need to specify types when declaring a variable, it will be inferred from the right hand side of the expression:
``` Ruby
var a = 5.5; // variable type inferred as float
var b = 0;   // variable type inferred as int
```

### Return type deduction
You don't have to write out return types either, the compiler is smart enough to figure it out:
``` Ruby
def foo()
    var i = 42;
    return i; // return type deduced to int
end

def bar()
    return 0.5; // return type deduced to float
end
```

### Static typing
Implicit conversions are compiler errors:
``` Ruby
var a = 4;
var b = 5.4342;
var v = a + b; // Error: attempt to add int and float
```
