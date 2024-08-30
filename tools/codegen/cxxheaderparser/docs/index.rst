.. cxxheaderparser documentation master file, created by
   sphinx-quickstart on Thu Dec 31 00:46:02 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

cxxheaderparser
===============

A pure python C++ header parser that parses C++ headers in a mildly naive
manner that allows it to handle many C++ constructs, including many modern
(C++11 and beyond) features.

.. warning:: cxxheaderparser intentionally does not use a C preprocessor by
             default. If you are parsing code with macros in it, you need to
             provide a preprocessor function in :py:class:`.ParserOptions`.

             .. seealso:: :py:attr:`cxxheaderparser.options.ParserOptions.preprocessor`

.. _pcpp: https://github.com/ned14/pcpp

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   tools
   simple
   custom
   types



Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

