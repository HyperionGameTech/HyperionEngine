Tools
=====

There are a variety of command line tools provided by the cxxheaderparser
project.

dump tool
---------

Dump data from a header to stdout

.. code-block:: sh

    # pprint format
    python -m cxxheaderparser myheader.h

    # JSON format
    python -m cxxheaderparser --mode=json myheader.h

    # dataclasses repr format
    python -m cxxheaderparser --mode=repr myheader.h

    # dataclasses repr format (formatted with black)
    python -m cxxheaderparser --mode=brepr myheader.h

Anything more than that and you should use the python API, start with the 
:ref:`simple API <simple>` first. 

test generator
--------------

To generate a unit test for cxxheaderparser:

* Put the C++ header content in a file
* Run the following:

.. code-block:: sh

    python -m cxxheaderparser.gentest FILENAME.h TESTNAME

You can copy/paste the stdout to one of the test files in the tests directory.