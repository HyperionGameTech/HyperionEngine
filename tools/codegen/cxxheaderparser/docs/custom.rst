Custom parsing
==============

For many users, the data provided by the simple API is enough. In some advanced
cases you may find it necessary to use this more customizable parsing mechanism.

First, define a visitor that implements the :class:`CxxVisitor` protocol. Then
you can create an instance of it and pass it to the :class:`CxxParser`.

.. code-block:: python

    visitor = MyVisitor()
    parser = CxxParser(filename, content, visitor)
    parser.parse()

    # do something with the data collected by the visitor

Your visitor should do something with the data as the various callbacks are
called. See the :class:`SimpleCxxVisitor` for inspiration.

API
---

.. automodule:: cxxheaderparser.parser
   :members:
   :undoc-members:

.. automodule:: cxxheaderparser.visitor
   :members:
   :undoc-members:

Parser state 
------------

.. automodule:: cxxheaderparser.parserstate
   :members:
   :undoc-members:

Preprocessor
------------

.. automodule:: cxxheaderparser.preprocessor
   :members:
   :undoc-members:
