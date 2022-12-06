<p align="right">
    <img src="http://img.shields.io/badge/License-MIT-blue.svg?style=flat"/>
</p>
<p align="center"><img src="docs/kilite.png" height="128px" /></p>
<p align="center">
Looks like JavaScript, feels like Ruby, and it is a script language fitting in C programmers.
</p>

# Kilite

This is like a lightweight Kinx but more flexible, portable, and faster.
Mainly, we will provide the following features for your help.

* To make it flexible, portable, and faster than Kinx.
  * This program is a successor of the Kinx project, so I'll support most of Kinx features.
  * Moreover, the performance or various points will be improved.
* To output a C source code to make an executable from that.
  * This is a new feature not in Kinx.
  * This feature will be helpful to make the executable from dynamically typed language script.
  * But you need another C compiler to make it.

## Motivation

[Kinx](https://github.com/Kray-G/kinx) is very useful for me. It's a really important project for me and actually I love it. However, unfortunately I feel now I have some difficulities to improve it more at least as long as it is. This means I'd need to remake it from its architecture if I wanted to make it improve more.

That's why I decided to try to making a new design of a programming language that I really wanted with a new architecture, athough I am going to keep using Kinx as well also in future.

## Usecases

There are some usecases that I want to try.
It would be roughly 4 patterns as below.

1.  To run the script as it is on the fly.
2.  To output C source code to make an executable.
3.  To generate the executable from your script directly by calling another compiler intrnally.
4.  To compile the script separatedly and to run the code with those precompiled codes.

### Running the Script

This will work with the current version.

    $ ./kilite file.klt

### Outputting C Code and Making the Executable

    $ ./kilite --cfull file.klt > file.c
    (file.c)
    $ gcc -o file file.c -L. -lkilite -lm
    (file)
    $ cl /Fefile.exe file.c kilite.lib
    (file.exe)
    $ ./file

Note that you need the actual compiler like gcc on Linux, cl.exe on Windows, or something to make an actual executable.
Instead, you can use the compiler you want.

### Generating the Executable Directly

    $ ./kilite -X file.klt
    (file or file.exe)
    $ ./file

Note that, also in this case, you need the actual compiler like gcc on Linux, cl.exe on Windows.
By default, it's cl.exe on Windows and gcc on Linux.
But if you want, tcc is also available.

### Compiling the Script Separatedly

*Note that this is not implemented yet.*

    $ ./kilite -c file1.klt
    (file1.kc)
    $ ./kilite -c file2.klt
    (file2.kc)
    $ ./kilite file1.kc file2.kc

## Benchmark

Now, I did benchmark with some script languages because even the current version of kilite can run the code like a fibonacci.
That's why I'll show it below. The target program is the 38th result of a fibonacci as usual.

### On Windows

|                    | Version                | Time  | Result   | Remark                           |
| ------------------ | ---------------------- | ----- | -------- | -------------------------------- |
| C (-O3)            | gcc 8.1.0              | 0.14s | 39088169 | Simple C code.                   |
| C (-O2)            | VS2019                 | 0.17s | 39088169 | Simple C code.                   |
| Kilite(C compiled) | (beta/gcc MinGW 8.1.0) | 0.17s | 39088169 | Compiled the output from Kilite. |
| Kilite(C compiled) | (beta/cl VS2019)       | 0.22s | 39088169 | Compiled the output from Kilite. |
| luajit             | 2.1.0-beta3            | 0.35s | 39088169 | -                                |
| PyPy               | 7.3.9                  | 0.40s | 39088169 | -                                |
| Kinx(native)       | 1.1.1                  | 0.57s | 39088169 | -                                |
| Kilite             | (beta)                 | 0.68s | 39088169 | -                                |
| Lua                | 5.4.4                  | 2.59s | 39088169 | -                                |
| Ruby               | 3.1.2p20               | 4.02s | 39088169 | -                                |
| Kinx               | 1.1.1                  | 5.18s | 39088169 | -                                |
| Python             | 3.11.0                 | 5.94s | 39088169 | -                                |

### On Linux

|                    | Version           | Time   | Result   | Remark                           |
| ------------------ | ----------------- | ------ | -------- | -------------------------------- |
| C (-O3)            | gcc 11.2.0        | 0.062s | 39088169 | Simple C code.                   |
| Kilite(C compiled) | (beta/gcc 11.2.0) | 0.125s | 39088169 | Compiled the output from Kilite. |
| luajit             | 2.1.0-beta3       | 0.318s | 39088169 | -                                |
| PyPy               | 7.3.9             | 0.330s | 39088169 | -                                |
| Kinx(native)       | 1.1.1             | 0.389s | 39088169 | -                                |
| Kilite             | (beta)            | 0.622s | 39088169 | -                                |
| Lua                | 5.4.4             | 2.526s | 39088169 | -                                |
| Ruby               | 3.0.2p107         | 2.576s | 39088169 | -                                |
| Kinx               | 1.1.1             | 4.657s | 39088169 | -                                |
| Python             | 3.10.6            | 7.461s | 39088169 | -                                |

### Result

There is some differences between on Windows and on Linux, but luajit and PyPy was very fast and amazing. That's exactly JIT as expected.
As for Kilite, this is almost the result that I wanted, but it could be slower by increasing the code in the future.
I will try to keep the perfrmance even if the code would be more complex.

The source code for each language is shown below.

#### Kinx/Kilite

Kilite is now already supported the `System.println` method as well as Kinx.
And when I outputted the C source code and compiled it, that was super fast.

    function fib(n) {
        if (n < 2) return n;
        return fib(n - 2) + fib(n - 1);
    }
    System.println(fib(38));

#### Kinx(native)

Kinx `native` was super fast, but we have to specify the `native` everytime.

    native fib(n) {
        if (n < 2) return n;
        return fib(n - 2) + fib(n - 1);
    }
    System.println(fib(38));

#### C

This is just a simple C code.

    #include <stdio.h>

    int fib(int n) {
        if (n < 2) return n;
        return fib(n - 2) + fib(n - 1);
    }

    int main()
    {
        printf("%d\n", fib(38));
    }

#### Ruby

Ruby is now much faster than before.

    def fib(n)
        return n if n < 2
        fib(n - 2) + fib(n - 1)
    end
    puts fib(38)

#### Lua/luajit

Lua is almost same as Ruby about the performance, but luajit is amazing and super fast.

    function fib(n)
        if n < 2 then return n end
        return fib(n - 2) + fib(n - 1)
    end
    print(fib(38))

#### Python/PyPy

Python was slow, but PyPy was very fast.

    def fib(n):
        if n < 2:
            return n
        return fib(n-2) + fib(n-1)
    print(fib(38))

## TODO

I will note the followings as I don't forget it.

* [ ] Support almost all basic functionalities first.
  * [ ] Main structures of source code.
    * [x] Make the Fibonacci benchmark work.
    * [x] Make the factorial recursive function work with big integer.
    * [x] All basic comparisons including `<=>`.
    * [x] `&&` and `||` as a shortcut operator.
      * [x] `&&=` and `||=` as a shortcut operator.
      * [x] `??` as a shortcut operator.
      * [x] `??=` as a shortcut operator.
    * [x] String operations.
    * [x] Array operations.
    * [x] Object operations.
    * [ ] Range operations.
      * [x] Fiber class.
        * [x] Coroutine with `yield`.
        * [x] Return value from `yield`.
      * [ ] Enumerator module.
      * [ ] Range class.
        * [x] Range for integer.
        * [ ] Range for string.
        * [ ] Range for others.
      * [ ] Range index for String, Binary, Array, and Range.
      * [ ] Range at the case value.
      * [x] Range in `for-in`.
    * [x] Support a `switch-case` statement.
    * [x] Support the loop like `for`, `while`, and `do-while`.
    * [x] `for-in` for Array.
    * [x] `for-in` for Object.
    * [x] `for-in` for Range.
    * [x] `if` modifier.
    * [x] `break` and `continue`.
      * [x] with label.
    * [x] Anonymous function in an expression.
    * [x] Easy print method.
  * [x] Namespaces, classes, modules, functions, inheritance, and mix-in mechanism.
    * [x] Namespace works.
    * [x] Class definition.
      * [x] Call `initialize` method when creating the instance if the method exists.
      * [x] Support the `instanceOf` method.
    * [x] The `new` operator.
    * [x] Module definition.
    * [x] The `mixin` statement.
    * [x] `@` is the same as the shorten word of `this.`.
  * [x] Exception.
    * [x] throwing an exception.
    * [x] Stack trace.
    * [x] `try`, `catch`, and `finally`.
    * [x] MethodMissing for global scope.
    * [ ] MethodMissing for class methods.
  * [ ] Special object.
    * [ ] Integer
    * [ ] String
    * [ ] Binary
    * [ ] Array/Object
* [ ] Notes for a special specifications of the language.
  * [x] The last argument of function call could be outside arguments list if it's a block.
  * [x] Destructuring assignment.
    * [x] Pattern matching in assignment.
    * [x] Destructuring assignment in function arguments.
  * [ ] `case-when` expression.
  * [x] Formatter object.
* [ ] Libraries support.
  * [ ] Useful Libraries like Zip, Xml, PDF, or something.

There could be something I've forgotten. I'll add it when I remember it.

In addition, the followings are the task list for the current implementation.

* [ ] Remaining items.
  * [ ] Error recovery during parsing like a panic mode.
  * [ ] Error report for reassigning a value into a variable of `const`.
  * [x] `isDefined` property.

## Others

### Goal

Below is the goal I wanted to achive. Some of them has been already done by Kinx, but some hasn't.

* Flexible
  * Useful gradual typing.
    * You can use it as a dynamically typed language, like you know duck-typing.
    * However, you can use it also as a statically typed language for safety.
  * Scripting and compiling.
    * You can run it on the fly very easily.
    * On the other hand, you can compile it before running it.
* Portable
  * The code will be run on several platforms like Windows, linux, and so on.
* Faster
  * I don't know how faster it is even if I tried to remake it, but I will challenge it.

### Design Concept

To meet the goal, I will try with followings.

* As a backend, [MIR](https://github.com/vnmakarov/mir) will be used.
  * This is an amazing JIT engine.
    * Optimizations including a function inlining are highly available.
    * Execution from C source code is also already supported.
  * Various platforms are supported.
    * Not only x86_64, but also PPC64, aarch64, etc.
  * It is easy to hold the code as a library, like it's a .bmir file.
    * Compiling separatedly, you can execute it all together later.
    * You can write the library by C language if you need, and use it as a .bmir as it is.

### `native`

Note that `native` keyword will be no longer supported because Kilite will work automatically with a similar way thought it's a little different.
For example, as you looked at above, the Kilite(beta-x) is working with a specialized method for 64bit integers and it has been already optimized.
It's like `native` in Kinx, but we don't have to use some keywords like `native` and it will work automatilly as we write it naturally.
