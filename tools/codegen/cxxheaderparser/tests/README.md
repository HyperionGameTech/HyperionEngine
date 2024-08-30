Tests
=====

To run the tests, install `cxxheaderparser` and `pytest`, then just run:

    pytest

Adding new tests
----------------

There's a helper script in cxxheaderparser explicitly for generating many of the
unit tests in this directory. To run it:

* Create a file with your C++ content in it
* Run `python -m cxxheaderparser.gentest FILENAME.h some_name` 
* Copy the stdout to one of these `test_*.py` files

Content origin
--------------

* Some are scraps of real code derived from various sources
* Some were derived from the original `CppHeaderParser` tests
* Some have been derived from examples found on https://en.cppreference.com,
  which are available under Creative Commons Attribution-Sharealike 3.0
  Unported License (CC-BY-SA)
