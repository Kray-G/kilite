# Comparison by Equation

## Overview

Operators for equation are `==` and `!=`.

```javascript
var a = 0x05;
var b = a == 0x06;   // 5 == 6 => false means 0
```

If the type is different, it will be compared after converting by the table below.

|         |    Integer    | Double | String        |
| :-----: | :-----------: | :----: | ------------- |
| Integer |    Integer    | Double | String by dec |
| Double  |    Double     | Double | -             |
| String  | String by dec |   -    | String        |

Comparing between Integer and String is done as stringified by decimal numbers.

## Examples

### Example 1. Use `==` for Integers

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("==", &(a, b) => a == b, 0x05, 0x06));
System.println(test("==", &(a, b) => a == b, 99, 99));
System.println(test("==", &(a, b) => a == b, 100, 11));
System.println(test("==", &(a, b) => a == b, null, 0x05));
System.println(test("==", &(a, b) => a == b, 0x07, null));
```

#### Result

```
::        5 ==        6 => false
::       99 ==       99 => true
::      100 ==       11 => false
:: ((null)) ==        5 => false
::        7 == ((null)) => false
```

### Example 2. Use `==` between Integer and Double

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("==", &(a, b) => a == b, 1, 1.0));
```

#### Result

```
::        1 ==        1 => true
```

### Example 3. Use `==` between Integer and String

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("==", &(a, b) => a == b, 0xff, "255"));
System.println(test("==", &(a, b) => a == b, 0xff, "ff"));
```

#### Result

```
::      255 ==      255 => true
::      255 ==       ff => false
```

### Example 4. Use `==` between String and Integer

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("==", &(a, b) => a == b, "255", 0xff));
System.println(test("==", &(a, b) => a == b, "ff", 0xff));
```

#### Result

```
::      255 ==      255 => true
::       ff ==      255 => false
```

### Example 5. Use `!=` for Integers

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("!=", &(a, b) => a != b, 0x05, 0x06));
System.println(test("!=", &(a, b) => a != b, 99, 99));
System.println(test("!=", &(a, b) => a != b, 100, 11));
System.println(test("!=", &(a, b) => a != b, null, 0x05));
System.println(test("!=", &(a, b) => a != b, 0x07, null));
```

#### Result

```
::        5 !=        6 => true
::       99 !=       99 => false
::      100 !=       11 => true
:: ((null)) !=        5 => true
::        7 != ((null)) => true
```

### Example 6. Use `!=` between Integer and Double

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("!=", &(a, b) => a != b, 1, 1.0));
```

#### Result

```
::        1 !=        1 => false
```

### Example 7. Use `!=` between Integer and String

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("!=", &(a, b) => a != b, 0xff, "255"));
System.println(test("!=", &(a, b) => a != b, 0xff, "ff"));
```

#### Result

```
::      255 !=      255 => false
::      255 !=       ff => true
```

### Example 8. Use `!=` between String and Integer

#### Code

```javascript
function test(label, f, a, b) {
    return ":: %8d %s %8d => %d" % a % label % b % f(a, b);
}

System.println(test("!=", &(a, b) => a != b, "255", 0xff));
System.println(test("!=", &(a, b) => a != b, "ff", 0xff));
```

#### Result

```
::      255 !=      255 => false
::       ff !=      255 => true
```
