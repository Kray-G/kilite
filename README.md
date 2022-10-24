# Kilite

Lightweight Kinx but more flexible, portable, and faster.

## Motivation

[Kinx](https://github.com/Kray-G/kinx) is very useful for me. It's a really important project for me and I actually love it. However, unfortunately I feel I have done almost all things in the current architecture. And I also feel it's dfficult to upgrade it at least as long as it is. This means I would have to remake the architecture if I wanted to make it improve more.

That's why I decided to try to making a new design of a programming language that I really wanted with a new architecture, athough I am going to keep using Kinx as well also in future.

## Goal

Below is the goal we wanted to achive. Some of them has been already done by Kinx, but some hasn't.

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
  * It is easy to hold the code as a library, like it's .bmir file.
    * Compiling separatedly, you can execute it all together later.
    * You can write the library by C language if you need, and use it as .bmir as it is.

## TODO

I will note the followings as I don't forget it.

* [ ] Support almost all basic functionalities first.
  * [ ] Main structures of source code.
  * [ ] Class, module, function, inheritance, mix-in mechanism.
  * [ ] Statement and Expression.
  * [ ] Exception.
* [ ] Notes for a special specifications of the language.
  * [ ] The last argument of function call could be outside arguments list if it's a block.
  * [ ] Object element's direct assignment.
  * [ ] 'Case-When' expression.

Note that the `native` keyword will be no longer supported because this solution is always using a native-call compilation.
