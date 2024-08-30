# Note: testcases generated via `python -m cxxheaderparser.gentest`
# .. and modified

import inspect
import typing

from cxxheaderparser.parser import CxxParser
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    ParsedData,
    SClassBlockState,
    SExternBlockState,
    SNamespaceBlockState,
    SimpleCxxVisitor,
)

from cxxheaderparser.types import (
    ClassDecl,
    Function,
    FundamentalSpecifier,
    Method,
    NameSpecifier,
    PQName,
    Type,
)

#
# ensure extern block is skipped
#


class SkipExtern(SimpleCxxVisitor):
    def on_extern_block_start(self, state: SExternBlockState) -> typing.Optional[bool]:
        return False


def test_skip_extern():
    content = """
      void fn1();

      extern "C" {
          void fn2();
      }

      void fn3();
    """

    v = SkipExtern()
    parser = CxxParser("<str>", inspect.cleandoc(content), v)
    parser.parse()
    data = v.data

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn3")]),
                    parameters=[],
                ),
            ]
        )
    )


#
# ensure class block is skipped
#


class SkipClass(SimpleCxxVisitor):
    def on_class_start(self, state: SClassBlockState) -> typing.Optional[bool]:
        if getattr(state.class_decl.typename.segments[0], "name", None) == "Skip":
            return False
        return super().on_class_start(state)


def test_skip_class() -> None:
    content = """
      void fn1();

      class Skip {
          void fn2();
      };

      class Yup {
          void fn3();
      };

      void fn5();
    """
    v = SkipClass()
    parser = CxxParser("<str>", inspect.cleandoc(content), v)
    parser.parse()
    data = v.data

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Yup")], classkey="class"
                        )
                    ),
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn3")]),
                            parameters=[],
                            access="private",
                        )
                    ],
                ),
            ],
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn5")]),
                    parameters=[],
                ),
            ],
        )
    )


#
# ensure namespace 'skip' is skipped
#


class SkipNamespace(SimpleCxxVisitor):
    def on_namespace_start(self, state: SNamespaceBlockState) -> typing.Optional[bool]:
        if "skip" in state.namespace.names[0]:
            return False

        return super().on_namespace_start(state)


def test_skip_namespace():
    content = """
      void fn1();

      namespace skip {
          void fn2();

          namespace thistoo {
              void fn3();
          }
      }

      namespace ok {
          void fn4();
      }

      void fn5();
    """
    v = SkipNamespace()
    parser = CxxParser("<str>", inspect.cleandoc(content), v)
    parser.parse()
    data = v.data

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn1")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn5")]),
                    parameters=[],
                ),
            ],
            namespaces={
                "ok": NamespaceScope(
                    name="ok",
                    functions=[
                        Function(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(segments=[NameSpecifier(name="fn4")]),
                            parameters=[],
                        )
                    ],
                ),
            },
        )
    )
