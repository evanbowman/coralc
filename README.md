# coralc

Coralc is the compiler for the Coral programming language. I've been working on Coral partly out of interest in language design, partly so that I can have a source language to write a compiler for without being bound to the grammar of an existing language. As of right now, Coral supports a number of features that are becomming common in modern languages, including type inferencing and return type deduction. For example:
```
def main()
    var v = 5.5; // variable type inferred as float
    return 0;    // return type deduced to int
end


def foo()
    var v = true; // variable type inferred as float
    for i in 0..5 do
        return i; // return type deduced to int
    end
    return v;     // This would throw an error, mismatched return types
end
```
 
