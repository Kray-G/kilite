
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

Statement is almost near the C or JavaScript syntax.

#### Basic Statement

A declaration and expression statement is a basic statement of Kilite.
Some of expression rules will be described in `expression` section.

*   [declaration](statement/declaration.md)

#### Flow Control

##### Block

*   [block](statement/block.md)

##### Branch

*   [if-else](statement/if_else.md)

##### Loop

*   [while](statement/while.md)
*   [do-while](statement/do_while.md)
*   [for](statement/for.md)

##### Jump

*   [return](statement/return.md)

### Definitions

Definitions are structured as a Function definition, a Class definition, and a Module definition.
As a special object derived from Function, there are Lambda, Closure, and Fiber.

#### Object Definitions

*   [Function](definition/function.md)
*   [Class](definition/class.md)
