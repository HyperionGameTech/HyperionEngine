cxxheaderparser
===============

A pure python C++ header parser that parses C++ headers in a mildly naive
manner that allows it to handle many C++ constructs, including many modern
(C++11 and beyond) features.

This is a complete rewrite of the `CppHeaderParser` library. `CppHeaderParser`
is really useful for some tasks, but it's implementation is a truly terrible
ugly hack built on top of other terrible hacks. This rewrite tries to learn
from `CppHeaderParser` and leave its ugly baggage behind.

Goals:

* Parse syntatically valid C++ and provide a useful (and documented!) pure
  python API to work with the parsed data
* Process incomplete headers (doesn't need to process includes)
* Provide enough information for binding generators to wrap C++ code
* Handle common C++ features, but it may struggle with obscure or overly
  complex things (feel free to make a PR to fix it!)

Non-goals:

* **Does not produce a full AST**, use Clang if you need that
* **Not intended to validate C++**, which means this will not reject all
  invalid C++ headers! Use a compiler if you need that
* **No C preprocessor substitution support implemented**. If you are parsing
  headers that contain macros, you should preprocess your code using the
  excellent pure python preprocessor [pcpp](https://github.com/ned14/pcpp)
  or your favorite compiler
  * See `cxxheaderparser.preprocessor` for how to use
* Probably won't be able to parse most IOCCC entries

There are two APIs available:

* A visitor-style interface to build up your own custom data structures 
* A simple visitor that stores everything in a giant data structure

Live Demo
---------

A pyodide-powered interactive demo is at https://robotpy.github.io/cxxheaderparser/

Documentation
-------------

Documentation can be found at https://cxxheaderparser.readthedocs.io

Install
-------

Requires Python 3.6+, no non-stdlib dependencies if using Python 3.7+.

```
pip install cxxheaderparser
```

Usage
-----

To see a dump of the data parsed from a header:

```
# pprint format
python -m cxxheaderparser myheader.h

# JSON format
python -m cxxheaderparser --mode=json myheader.h

# dataclasses repr format
python -m cxxheaderparser --mode=repr myheader.h

# dataclasses repr format (formatted with black)
python -m cxxheaderparser --mode=brepr myheader.h
```

See the documentation for anything more complex.

Bugs
----

This should handle even complex C++ code with few problems, but there are
almost certainly weird edge cases that it doesn't handle. Additionally,
not all C++17/20 constructs are supported yet (but contributions welcome!).

If you find an bug, we encourage you to submit a pull request! New
changes will only be accepted if there are tests to cover the change you
made (and if they don’t break existing tests).

Author
------

cxxheaderparser was created by Dustin Spicuzza

Credit
------

* Partially derived from and inspired by the `CppHeaderParser` project
  originally developed by Jashua Cloutier
* An embedded version of PLY is used for lexing tokens
* Portions of the lexer grammar and other ideas were derived from pycparser
* The source code is liberally sprinkled with comments containing C++ parsing
  grammar mostly derived from the [Hyperlinked C++ BNF Grammar](https://www.nongnu.org/hcb/)
* cppreference.com has been invaluable for understanding many of the weird
  quirks of C++, and some of the unit tests use examples from there
* [Compiler Explorer](godbolt.org) has been invaluable for validating my 
  understanding of C++ by allowing me to quickly type in quirky C++
  constructs to see if they actually compile

License
-------

BSD License
