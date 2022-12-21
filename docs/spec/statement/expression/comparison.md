# Comparison

## Overview

Comparison operators are `<`, `>`, `<=`, and `>=`.

```javascript
var a = 0x05;
var b = a < 0x06;   // 5 < 6 => true means 1
```

## Examples

### Example 1. Simple use of `<`

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("<", &(a, b) => a < b, 0x05, 0x06));
System.println(test("<", &(a, b) => a < b, 99, 99));
System.println(test("<", &(a, b) => a < b, 100, 11));
System.println(test("<", &(a, b) => a < b, null, 0x05));
System.println(test("<", &(a, b) => a < b, 0x07, null));
```

#### Result

```
::        5 <        6 => true
::       99 <       99 => false
::      100 <       11 => false
:: ((null)) <        5 => true
::        7 < ((null)) => false
```

### Example 2. Simple use of `<=`

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("<=", &(a, b) => a <= b, 0x05, 0x06));
System.println(test("<=", &(a, b) => a <= b, 99, 99));
System.println(test("<=", &(a, b) => a <= b, 100, 11));
System.println(test("<=", &(a, b) => a <= b, null, 0x05));
System.println(test("<=", &(a, b) => a <= b, 0x07, null));
```

#### Result

```
::        5 <=        6 => true
::       99 <=       99 => true
::      100 <=       11 => false
:: ((null)) <=        5 => true
::        7 <= ((null)) => false
```

### Example 3. Simple use of `>`

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test(">", &(a, b) => a > b, 0x05, 0x06));
System.println(test(">", &(a, b) => a > b, 99, 99));
System.println(test(">", &(a, b) => a > b, 100, 11));
System.println(test(">", &(a, b) => a > b, null, 0x05));
System.println(test(">", &(a, b) => a > b, 0x07, null));
```

#### Result

```
::        5 >        6 => false
::       99 >       99 => false
::      100 >       11 => true
:: ((null)) >        5 => false
::        7 > ((null)) => true
```

### Example 4. Simple use of `<=`

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test(">=", &(a, b) => a >= b, 0x05, 0x06));
System.println(test(">=", &(a, b) => a >= b, 99, 99));
System.println(test(">=", &(a, b) => a >= b, 100, 11));
System.println(test(">=", &(a, b) => a >= b, null, 0x05));
System.println(test(">=", &(a, b) => a >= b, 0x07, null));
```

#### Result

```
::        5 >=        6 => false
::       99 >=       99 => true
::      100 >=       11 => true
:: ((null)) >=        5 => false
::        7 >= ((null)) => true
```

### Example 5. Big Integer of `<`

#### Code

```javascript
const BI = 0x7fffffffffffffff + 2;
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("<", &(a, b) => a < b, BI, 0x06));
System.println(test("<", &(a, b) => a < b, BI, BI));
System.println(test("<", &(a, b) => a < b, 100, BI));
System.println(test("<", &(a, b) => a < b, null, BI));
System.println(test("<", &(a, b) => a < b, BI, null));
```

#### Result

```
:: 9223372036854775809 <        6 => false
:: 9223372036854775809 < 9223372036854775809 => false
::      100 < 9223372036854775809 => true
:: ((null)) < 9223372036854775809 => true
:: 9223372036854775809 < ((null)) => false
```

### Example 6. Big Integer of `<=`

#### Code

```javascript
const BI = 0x7fffffffffffffff + 2;
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("<=", &(a, b) => a <= b, BI, 0x06));
System.println(test("<=", &(a, b) => a <= b, BI, BI));
System.println(test("<=", &(a, b) => a <= b, 100, BI));
System.println(test("<=", &(a, b) => a <= b, null, BI));
System.println(test("<=", &(a, b) => a <= b, BI, null));
```

#### Result

```
:: 9223372036854775809 <=        6 => false
:: 9223372036854775809 <= 9223372036854775809 => true
::      100 <= 9223372036854775809 => true
:: ((null)) <= 9223372036854775809 => true
:: 9223372036854775809 <= ((null)) => false
```

### Example 7. Big Integer of `>`

#### Code

```javascript
const BI = 0x7fffffffffffffff + 2;
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test(">", &(a, b) => a > b, BI, 0x06));
System.println(test(">", &(a, b) => a > b, BI, BI));
System.println(test(">", &(a, b) => a > b, 100, BI));
System.println(test(">", &(a, b) => a > b, null, BI));
System.println(test(">", &(a, b) => a > b, BI, null));
```

#### Result

```
:: 9223372036854775809 >        6 => true
:: 9223372036854775809 > 9223372036854775809 => false
::      100 > 9223372036854775809 => false
:: ((null)) > 9223372036854775809 => false
:: 9223372036854775809 > ((null)) => true
```

### Example 8. Big Integer of `>=`

#### Code

```javascript
const BI = 0x7fffffffffffffff + 2;
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test(">=", &(a, b) => a >= b, BI, 0x06));
System.println(test(">=", &(a, b) => a >= b, BI, BI));
System.println(test(">=", &(a, b) => a >= b, 100, BI));
System.println(test(">=", &(a, b) => a >= b, null, BI));
System.println(test(">=", &(a, b) => a >= b, BI, null));
```

#### Result

```
:: 9223372036854775809 >=        6 => true
:: 9223372036854775809 >= 9223372036854775809 => true
::      100 >= 9223372036854775809 => false
:: ((null)) >= 9223372036854775809 => false
:: 9223372036854775809 >= ((null)) => true
```
