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

## Usecases

There are some usecases that I want to try.
It would be roughly 4 patterns as below.

1.  To run the script as it is on the fly.
2.  To compile the script separatedly and to run the code with those precompiled codes.
3.  To call the C code directly from the script function.
4.  To call the script from the C code as well.

### Running the Script

This will work with the current version.

    $ ./kilite file.kl

### Compiling the Script Separatedly

*Note that this is not implemented yet.*

    $ ./kilite -c file1.kl
    (file1.kc)
    $ ./kilite -c file2.kl
    (file2.kc)
    $ ./kilite file1.kc file2.kc

## Benchmark

Now, I did benchmark with some script languages because the current version of kilite can run the code if it's like a fibonacci.
That's why I'll show it below.
The target program is the 38th result of a fibonacci as usual.

|              | Version     | Time  | Result   |
| ------------ | ----------- | ----- | -------- |
| luajit       | 2.1.0-beta3 | 0.42s | 39088169 |
| PyPy         | 7.3.9       | 0.42s | 39088169 |
| Kinx(native) | 1.1.1       | 0.57s | 39088169 |
| Kilite       | (beta-x)    | 0.60s | 39088169 |
| Kilite       | (beta)      | 1.67s | 39088169 |
| Lua          | 5.4.4       | 2.78s | 39088169 |
| Ruby         | 3.1.2p20    | 3.30s | 39088169 |
| Kinx         | 1.1.1       | 5.63s | 39088169 |
| Python       | 3.11.0      | 6.28s | 39088169 |

luajit and PyPy was very fast. That's exactly JIT as expected.
Kilite(beta-x) will create a specialized method for 64bit integers and it will automtically work, so it's very fast when it's using only a 64bit integer.
Even with Kilite(beta) that turned this optimization off, it's been 2x faster than Ruby, and 3.3x faster than Kinx.
This is the result as I wanted, but it could be slower by increasing the code in the future.
I will try to keep the perfrmance even if the code would be more complex.

The source code for each language is shown below.

#### Kinx/Kilite

Kilite has already supported `System.println` method as well as Kinx.

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
      * [ ] `&&=` and `||=` as a shortcut operator.
      * [ ] `??` as a shortcut operator.
      * [ ] `??=` as a shortcut operator.
    * [x] String operations.
    * [x] Array operations.
    * [x] Object operations.
    * [ ] Range operations.
      * [x] Fiber class.
        * [x] Coroutine with `yield`.
        * [x] Return value from `yield`.
      * [ ] Enumerator module.
      * [ ] Range class.
      * [ ] Range index for String, Binary, Array, and Range.
      * [ ] Range at the case value.
      * [ ] Range in `for-in`.
    * [ ] Support a `switch-case` statement.
    * [x] Support the loop like `for`, `while`, and `do-while`.
    * [ ] `for-in` for Array.
    * [ ] `for-in` for Object.
    * [ ] `for-in` for Range.
    * [x] `if` modifier.
    * [x] `break` and `continue`.
      * [x] with label.
    * [x] Anonymous function in an expression.
    * [x] Easy print method.
  * [ ] Namespaces, classes, modules, functions, inheritance, and mix-in mechanism.
    * [ ] Namespace works.
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
  * [ ] The last argument of function call could be outside arguments list if it's a block.
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
  * [ ] `isDefined` property.

## Others

### `native`

Note that `native` keyword will be no longer supported because Kilite will work automatically with a similar way thought it's a little different.
For example, as you looked at above, the Kilite(beta-x) is working with a specialized method for 64bit integers and it has been already optimized.
It's like `native` in Kinx, but we don't have to use some keywords like `native` and it will work automatilly as we write it naturally.
