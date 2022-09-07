# Open-CMSIS-Pack Development Tools and Libraries Coding Guidelines

## File encoding, line endings, indentation

### Encoding

All text files (source code, documentation) *shall* be stored using
**UTF-8 without BOM**.

### Line endings

Text files *shall* use **Unix line endings** by default (i.e. line feed only).
Text files *shall* end in a new-line character and the new-line character
cannot be immediately preceded by a backslash character (i.e. empty line).

Files only relevant for Windows *may* diverge from this rule if required,
e.g. for Windows Batch files.

### Maximum line length

The maximum length of a line *shall not* **exceed 120 characters** [1].

In documentation (e.g. Markdown or Java doc strings) a line *shall not*
**exceed 120 characters** [1].

In rare cases (e.g. static data in source files) it may not be well readable to
add line breaks based on line length. In such situations, it *may* be
acceptable to keep longer lines.

[1] Limiting the length of lines in text files helps reducing merge conflicts.

### Indentation

The indentation *shall* be done using **spaces** and not using tab characters
by default. The default indentation width *shall* be **2 space characters** if
not specified differently for a certain file type.

Certain file types (e.g. Unix Makefiles) *may* require tabs as delimiting
characters. Those are known exceptions to be taken.

## Folder and file names

Names of folders or files *shall* be **lower case** by default. Exceptions
may be given explicitly for certain types of files.

When diverging from this default rule one *must* be aware of different
behavior on supported operating systems:

- Windows is case insensitive
- Mac OS is case insensitive
- Linux (Unix) is case sensitive

On case sensitive systems (Linux) the names `file.txt`, `File.txt` and
`FILE.txt` are considered different files. One can have all three in a single
folder without issues. But on case insensitive systems (Windows/Mac) those
files are considered the same file. Accidentally adding `file.txt` *and*
`FILE.txt` from a Linux system into the Git repo shall cause issues on
checkout on Windows and Mac systems.

When referencing files (e.g. from documentation or build scripts) the file
path *must* be used as displayed (i.e. reflecting the exact case).

### Markdown files (.md)

Some special markdown files are commonly in upper case such as

- **README.md** used typically on repository top level as an entry point.
- **CONTENT.md** once per folder to briefly describe the folder content.

### Source Files (.cpp, .java)

Source files containing a class (e.g. in C++ or Java) *shall* be named
according to the class in camel case, e.g. C++ `MyClass` *shall* be declared in
`MyClass.h` and defined in `MyClass.cpp`.

## Folder structure

The repository and component folder structure *shall* follow widely known
patterns in order to make navigation intuitive.

The top level repository structure *shall* look like this:

- `build` temporary out-of-tree CMake build folder
- `docs` repository global documentation
- `external` third party dependencies
- `libs` library components
- `scripts` build and other helper scripts
- `test` multi-component integration or system level tests
- `tools` executable components

Each component in either `libs`, `tools` or `test` *shall* follow a pattern
suitable to the components programming language.

### C/C++ Components

Components written in C/C++ *shall* have a component level folder structure
like this:

- `docs` component level documentation, e.g. API description
- `include` public header files (intended to be included by other components)
- `src` source and private header files
- `test` test code (e.g. unit tests)

## GitIgnore File

The repository shall have only one [.gitignore](../.gitignore) file. All
untracked files or folders to be ignored shall be listed in this file.

## Copyright Notice

It is **mandatory** that you include the copyright notice on your work.

The aim of a copyright notice is to:

- Make it clear that the work is subject to copyright.
- Provide a means of identifying the copyright owner.
- Deter infringement or plagiarism.

The copyright notice should follow the format mentioned below.

```c++
/*
 * Copyright (c) <Copyright_Date> <Copyright_Text>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

Copyright_Date : Year of publication or a range of years.
Copyright_Text : Copyright author's or company's name. Additionally,
                 users can specify a "statement of rights". 
```

for e.g.

```c++
 /*
 * Copyright (c) 1985 John Miller
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
                OR
 /*
 * Copyright (c) 2020-2021 ABC Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
```

### Code style

All files containing source code *shall* be styled according to their language
style guide, see below.

Code style *may* be applied automatically before commit/check-in if an
appropriate pretty printer for the language is available.

Code style *may* be checked automatically on CI if a style checker is
available.

## Python (.py)

Python code *shall* be styled according to the
[**Style Guide for Python Code**](https://www.python.org/dev/peps/pep-0008/).

The code style *shall* be checked for compliance using
[**pycodestyle**](https://pypi.org/project/pycodestyle/).

## C++

C++17 *shall* be the preferred language standard.

### Header guards

Header guards *shall* be used to optimize build time and avoid multiple
inclusion errors.

Example `fooserver.h`:

```c++
#ifndef FOOSERVER_H
#define FOOSERVER_H

\\ Code

#endif /* FOOSERVER_H */
```

### Header order

Referencing Lakos' "Large Scale C++ Software Design" (section 3.2):
Latent usage errors can be avoided by ensuring that the .h file of a component
parses by itself – without externally-provided declarations or definitions.
The way to achieve this is to include headers in "from local to global" order:

1. The corresponding header for this .cpp file  
    The base class header for this .h file
2. Other headers from the same project
3. Headers from other own libraries (e.g. XmlTree, RteUtils)
4. "CrossPlatform.h" if needed
5. Headers from toolkit libraries (for example MFC, Xerces)
6. Standard C++ headers (for example, iostream, functional, etc.)
7. Standard C headers (for example, cstdint, dirent.h, etc.)

The above groups should be separated by a blank line.
Alphabetical sorting within a group is recommended.

Please note that files in XmlTreee, RteUtils and RteModel libraries already
follow the described rule.

### Standard headers

Standard C++ headers should be used instead of C ones where possible, e.g.
**\<cstring>** instead of **<string.h>**

### Relative includes

Specifying a path in an `#include` statement should be avoided:  
use `#include "bar.h"` instead of `#include "foo/bar.h"`

This makes the code independent from include file location.

In some situations however the relative includes cannot be avoided:

- system headers: `#include <sys/types.h>`
- third-party library headers: `#include <xercesc/dom/DOM.hpp>`

An `#include` statement may not contain paths with dots:  
neither `#include "./bar.h"` nor `#include "../foo.h"` are allowed.

### goto statements

No `goto` statements are allowed in the new code. All existing code where it
is used should be restructured.

### Code comments

Every function declaration should have a comment immediately preceding it that
describes what the function does and how to use it. In general, function
declaration comment describes **use of the function**. Comment in the
definition of a function describes **operation**.

Only C++ Doxygen style comments should be used in function declartaions, e.g.:

```c++
/**
 * @brief Opens the file.
 * @param file Fully qualified path of the file.
 * @return true if file opens successfully, otherwise false.
*/
bool OpenFile(const std::string& file);
```

### Defining struct and enum types

Only C++ style for `struct` and `enum` should be used in C++ code, e.g.:

```c++
struct Foo {
 int a;
 char *name;
}

enum Bar {
  FIRST,
  SECOND
};
```

C style definitions can only be used withing `extern "C"` blocks:

```c++
extern "c" {
  ...
  typedef struct {
    int a;
    char *name;
  } Foo;

  typedef enum {
    FIRST,
    SECOND
  } Bar;
  ...
} // extern "C"
```

The primary reason for this requirement is that typedef cannot be used for
forward declaration in C++.

Another aspect is that typedef cannot be used to define a struct member, the
following construction is not possible:

```c++
typedef struct {
  Foo* foo; // syntax error
  ...
} Foo;
```

And finally, it violates the C++ style.

### Self-made linked lists

Self-make linked lists like `struct Foo { struct Foo next*; ...}` are not
permitted in C++ code unless defined and used in legacy C code. Suitable C++
collections should be used: `std:list`, `std::set`, `std:vector` or `std:map`.

The reason is to improve robustness, enforce code reuse and decrease
maintenance overhead.

### Commented-out code

Unused code should be removed. It is not allowed to comment-out such code or
disable it using `#ifdef 0` preprocessor directives.

### Duplicated code

No duplicated (copy-paste) code is allowed. If a piece of code is used more
than once, a dedicated function should be created.

### One statement per line

Only one statement per line is allowed:

```c++
if(expr) return; // wrong
if(expr)
   return; // correct
if(expr) {
   return; // correct and even better
}
int res = a > b ? a : b;  // correct, still one statement
```

The main reason behind this rule is the ability to set an unconditional line
breakpoint for each statement.

### Boolean literals

Boolean values *shall* be represented by the keywords **true** and **false**
instead of numeric literals.
However these keywords *shall not* be used in comparisons because any nonzero
value is considered **true** and explicit tests against **true** and **false**
are generally inappropriate.

### Numeric literals

Unsigned numeric literals *shall* have unsigned suffixes to avoid type
conversion. Numeric literals suffixes *shall* be uppercase.

### Naming conventions

#### **Constants and `enum` values**

Constants and enum values should use `UPPERCASE_NAMES` with underscore
delimiter where appropriate. For instance:

```c++
#define MYDEF "mydef"
const int ULTIMATE_ANSWER = 42;
const string EMPTY_STRING;

enum Bar {
  FIRST,
  SECOND
};
```

#### **Type names**

All types defined via `class, struct, union, enum, typedef` keywords should use
`MixedCaseNames` starting with a capital letter. No underscores are allowed.

#### **Function and method names**

All functions including class methods should use `MixedCase()` names starting
with capital letters. No underscores are allowed. Exceptions to this rule is
overridden virtual methods inherited from standard or third-party classes.

#### **Variables**

All variables should use `lowercase` or `mixedCase` names starting with a small
letter. Underscores are only allowed in member variable prefixes.

Class member variables should use the following prefixes:

- `the` : singleton or single instance object, e.g. `theKernel`
- `s_`  : static members except constants, e.g. `s_instanceCount`
- `t_`  : variables whose sole role is to pass a value from one method to
          another, e.g. `t_mouseDownPoint`
- `m_`  : all other member variables, e.g. `m_children`

Variable names should be meaningful, in particular function arguments should be
self-explaining. Small names such as `i`, `j`, `c`, `ch` can only be used for
stack variables.

### Function arguments

Arguments in C++ are passed by value. It works good for integral types and
pointers. Other complex types such as collections should be explicitly passed
by reference:

- constant reference if value is not supposed to be modified in function
- non-constant reference if value should be modified

Consider the following function signature:

```c++
void Foo(list<string> values); // wrong: making copy
```

Calling `Foo` shall create a **COPY** of `values`causing performance penalty.
In works case it could even lead to stack overflow if `values` size is large
enough.

The following signature would be used instead:

```c++
void Foo(const list<string>& values); // correct
```

Using that signature ensures that only address of `values` is passed to the
function.

### C++ `auto` keyword

In most cases its is appropriate to use `auto` keyword in iterator
declarations, e.g. `auto it = aCollection.begin();`

However, care should be taken when using `auto` keyword in assignments where
R-Value is a collection or complex type. In such cases often `auto&` should be
used. For example, suppose there is a method:

```c++
const list<string>& GetValues();
```

Using `auto` in the following assignment:

```c++
auto values = GetValues();  // wrong: list<string>
```

is wrong: this assignment shall produce a **COPY** of the returned collection
since `values` type is `list<string>`. It could even lead to stack overflow if
collection size is large enough.

The correct expression should use `auto&`:

```c++
auto& values = GetValues(); // correct: const list<string>&
```
