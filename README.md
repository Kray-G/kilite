# Kilite

Lightweight Kinx but more flexible, portable, and faster.

## Motivation

[Kinx](https://github.com/Kray-G/kinx) is very useful for me. It's a really important project for me and actually I love it. However, unfortunately I feel now I have some difficulities to improve it more at least as long as it is. This means I'd need to remake it from its architecture if I wanted to make it improve more.

That's why I decided to try to making a new design of a programming language that I really wanted with a new architecture, athough I am going to keep using Kinx as well also in future.

## Goal

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

## Solution

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

## Benchmark

Now, I did benchmark with some script languages because the current version of kilite can run the code if it's like a fibonacci.
That's why I'll show it below.
The target program is the 38th result of a fibonacci as usual.

|              | Version     | Time  |
| ------------ | ----------- | ----- |
| luajit       | 2.1.0-beta3 | 0.42s |
| PyPy         | 7.3.9       | 0.42s |
| Kinx(native) | 1.1.1       | 0.57s |
| Kilite       | (beta)      | 1.67s |
| Lua          | 5.4.4       | 2.78s |
| Ruby         | 3.1.2p20    | 3.30s |
| Kinx         | 1.1.1       | 5.63s |
| Python       | 3.11.0      | 6.28s |

luajit and PyPy was very fast. That's exactly JIT as expected.
Kilite has already been 2x faster than Ruby, and 3.3x faster than Kinx.
In the future, it could be slower by increasing the code, but I will try to keep the perfrmance even if the code would be more complex.

## TODO

I will note the followings as I don't forget it.

* [ ] Support almost all basic functionalities first.
  * [ ] Main structures of source code.
    * [x] Make the Fibonacci benchmark work.
    * [x] Make the factorial recursive function work with big integer.
    * [x] All basic comparisons including `<=>`.
    * [x] String operations.
    * [x] Array operations.
    * [x] Object operations.
    * [ ] Range operations.
    * [x] Support the loop like `for`, `while`, and `do-while`.
    * [ ] `for-in` for Array.
    * [ ] `for-in` for Object.
    * [ ] `for-in` for Range.
  * [ ] Class, module, function, inheritance, mix-in mechanism.
  * [ ] Statement and Expression.
  * [ ] Exception.
* [ ] Notes for a special specifications of the language.
  * [ ] The last argument of function call could be outside arguments list if it's a block.
  * [ ] Object and array element's direct assignment.
  * [ ] Coroutine with `yield`.
  * [ ] 'Case-When' expression.
  * [ ] Support `native` keyword for high performance.
* Libraries support.
  * [ ] Useful Libraries like Zip, Xml, PDF, or something.

There could be something I've forgotten.

In addition, the followings are the task list for the current implementation.

* [ ] Remaining items.
  * [ ] Error recovery during parsing like a panic mode.
  * [ ] Error report for reassigning a value into a variable of `const`.

## Others

### `native`

I decided I would support the `native` keyword for more convenience solution to make the code faster. However, I don't know that this can make it actually.
I can't promise anything, but I'll challenge myself to do it.

Note that the current implementation will work with `native` but it's almost same as the code without `native`.
