
# Kilite Specification

## Overview

### How to Run

Set and confirm the path to the `kxtest` executable first.
After that, execute the command below under `kilite` repository's root to run the **SpecTest** of Kilite,
and **SpecTest** will automatically load a `.spectest` file and run the tests.

```
$ kxtest -v
```

## Contents

### Command Line

Not yet.

### Statement

### Statement

Statement is almost near the C or JavaScript syntax.
There are some of special statements that `yield` for Fiber, `mixin` for Module, and so on.

#### Basic Statement

A declaration and expression statement is a basic statement of Kinx.
Some of expression rules will be described in `expression` section.

*   [declaration](statement/declaration.md)
*   [enum](statement/enum.md)
*   [expression](statement/expression.md)
*   [mixin](statement/mixin.md)
*   [using](statement/using.md)

`mixin` is a special declaration used to include `Module` component.
This statement will be described also in `Module` section again.
And a declaration of a Function or a Class, etc, is described not in this section but in **Definitions** section.

#### Flow Control

Flow control is a branch, a loop, returning from a function, throwing a exception, etc.
But as for function call is described in `expression` because it is a part of expression.
Note that a flow is not changed by `block`, but `block` is included in this section.

##### Block

*   [block](statement/block.md)
*   [namespace](statement/namespace.md)

##### Branch

*   [if-else](statement/if_else.md)
*   [switch-case](statement/switch_case.md)
*   [switch-when](statement/switch_when.md)
*   [try-catch-finally](statement/try_catch_finally.md)

By the way, Kinx has supports a `case-when`, and the syntax of `case-when` is described in [expression](statement/expression.md) because it is an expression.

##### Loop

*   [while](statement/while.md)
*   [do-while](statement/do_while.md)
*   [for](statement/for.md)
*   [for-in](statement/for_in.md)

##### Jump

*   [return](statement/return.md)
*   [yield](statement/yield.md)
*   [throw](statement/throw.md)

### Definitions

Definitions are structured as a Function definition, a Class definition, and a Module definition.
As a special object derived from Function, there are Lambda, Closure, and Fiber.

#### Object Definitions

*   [Function](definition/function.md)
*   [Class](definition/class.md)
*   [Module](definition/module.md)
*   [Native](________NOT_IMPEMENTED_YET/definition/native.md)

#### Special Objects

*   [Lambda](definition/lambda.md)
*   [Closure](definition/closure.md)
*   [Fiber](definition/fiber.md)

### Type Check

The following properties are used to check the type of a variable.

*   [TypeCheck](________NOT_IMPEMENTED_YET/statement/expression/typecheck.md)

### Library

Kinx has many useful libraries and you can use those once you install this product.
Kinx goal is to become a glue between libraries, and Kinx will include many of useful libraries also in future.

#### Primitives

This is not a library but basic methods for a primitive data type,
but the description is included in this section because its feature is near a library.

*   [Integer](lib/primitive/integer.md)
*   [Double](lib/primitive/double.md)
*   [String](lib/primitive/string.md)
*   [Binary](lib/primitive/binary.md)
*   [Array](lib/primitive/array.md)

#### Functions

*   [eval()](NO_DOC/lib/function/eval.md) - *not documented yet*
*   [DefineException()](NO_DOC/lib/function/define_exception.md) - *not documented yet*

#### Standard Objects

Here are provided objects as a Kinx Standard.

##### Basic Objects

Basic objects are the list of components usually used in many products.
The functionality is very simple but powerful, so a lot of developpers will use those naturally.

*   [System](lib/basic/system.md)
*   [Getopt](NO_DOC/lib/basic/getopt.md) - *not documented yet*
*   [Iconv](NO_DOC/lib/basic/iconv.md) - *not documented yet*
*   [Colorize](NO_DOC/lib/basic/colorize.md) - *not documented yet*
*   [Math](NO_DOC/lib/basic/imath.md) - *not documented yet*
*   [File](NO_DOC/lib/basic/file.md) - *not documented yet*
*   [Directory](NO_DOC/lib/basic/directory.md) - *not documented yet*
*   [Regex](lib/basic/regex.md)
*   [Enumerable](NO_DOC/lib/basic/enumerable.md) - *not documented yet*
*   [Functional](NO_DOC/lib/basic/functional.md) - *not documented yet*
*   [Range](NO_DOC/lib/basic/range.md) - *not documented yet*
*   [DateTime](NO_DOC/lib/basic/datetime.md) - *not documented yet*
*   [Process](NO_DOC/lib/basic/process.md) - *not documented yet*
*   [JSON](NO_DOC/lib/basic/json.md) - *not documented yet*
*   [CSV/TSV](NO_DOC/lib/basic/csv_tsv.md) - *not documented yet*
*   [Xml](lib/basic/xml.md)
*   [Zip](NO_DOC/lib/basic/zip.md) - *not documented yet*
*   [SQLite](NO_DOC/lib/basic/sqlite.md) - *not documented yet*
*   [JIT](NO_DOC/lib/basic/jit.md) - *not documented yet*
*   [Parsek](NO_DOC/lib/basic/parsek.md) - *not documented yet*
*   [Clipboard](NO_DOC/lib/basic/clipboard.md) - *not documented yet*
*   [SemanticVersion](NO_DOC/lib/basic/semanticver.md) - (V1.1.0) *not documented yet*

##### Network Objects

Network library is now very few, but it will be increased in future
because now the networking library is needed by many developpers.

*   [Http](NO_DOC/lib/net/http.md) - *not documented yet*
*   [SSH](NO_DOC/lib/net/ssh.md) - *not documented yet*

I provide Http library only, but it is based on libcurl.
Therefore adding a library based on the same technology must be easy.
I strongly want **contributers**. How about you?

Note that the above documents do not have any test code because it is hard to test of networking.

##### Exception Objects

*   [Exception](NO_DOC/lib/primitive/exception.md) - *not documented yet*

#### Algorithm and Data Structure

Algorithm and Data structure examples are being described in the page below.

*   [Algorithm](NO_DOC/algorithm/README.md) - *not documented yet*

### Others

Some special specifications.

*   [Real Numbers](NO_DOC/others/realnumber.md) - *not documented yet*
*   [String#next](NO_DOC/others/string_next.md) - *not documented yet*
*   [String#isIntegerString](NO_DOC/others/string_isIntegerString.md) - V1.1.0) *not documented yet*
