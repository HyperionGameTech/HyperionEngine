# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.errors import CxxParseError
from cxxheaderparser.types import (
    Array,
    ClassDecl,
    EnumDecl,
    Enumerator,
    Field,
    FunctionType,
    FundamentalSpecifier,
    NameSpecifier,
    PQName,
    Parameter,
    Pointer,
    Reference,
    Token,
    Type,
    Value,
    Variable,
)
from cxxheaderparser.simple import ClassScope, NamespaceScope, ParsedData, parse_string

import pytest
import re


def test_var_unixwiz_ridiculous() -> None:
    # http://unixwiz.net/techtips/reading-cdecl.html
    #
    # .. "we have no idea how this variable is useful, but at least we can
    #     describe the type correctly"

    content = """
      char *(*(**foo[][8])())[];
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="foo")]),
                    type=Array(
                        array_of=Array(
                            array_of=Pointer(
                                ptr_to=Pointer(
                                    ptr_to=FunctionType(
                                        return_type=Pointer(
                                            ptr_to=Array(
                                                array_of=Pointer(
                                                    ptr_to=Type(
                                                        typename=PQName(
                                                            segments=[
                                                                FundamentalSpecifier(
                                                                    name="char"
                                                                )
                                                            ]
                                                        )
                                                    )
                                                ),
                                                size=None,
                                            )
                                        ),
                                        parameters=[],
                                    )
                                )
                            ),
                            size=Value(tokens=[Token(value="8")]),
                        ),
                        size=None,
                    ),
                )
            ]
        )
    )


def test_var_ptr_to_array15_of_ptr_to_int() -> None:
    content = """
      int *(*crocodile)[15];
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="crocodile")]),
                    type=Pointer(
                        ptr_to=Array(
                            array_of=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="int")]
                                    )
                                )
                            ),
                            size=Value(tokens=[Token(value="15")]),
                        )
                    ),
                )
            ]
        )
    )


def test_var_ref_to_array() -> None:
    content = """
      int abase[3];
      int (&aname)[3] = abase;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="abase")]),
                    type=Array(
                        array_of=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        ),
                        size=Value(tokens=[Token(value="3")]),
                    ),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="aname")]),
                    type=Reference(
                        ref_to=Array(
                            array_of=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            size=Value(tokens=[Token(value="3")]),
                        )
                    ),
                    value=Value(tokens=[Token(value="abase")]),
                ),
            ]
        )
    )


def test_var_ptr_to_array() -> None:
    content = """
      int zz, (*aname)[3] = &abase;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="zz")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="aname")]),
                    type=Pointer(
                        ptr_to=Array(
                            array_of=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            size=Value(tokens=[Token(value="3")]),
                        )
                    ),
                    value=Value(tokens=[Token(value="&"), Token(value="abase")]),
                ),
            ]
        )
    )


def test_var_multi_1() -> None:
    content = """
      int zz, (&aname)[3] = abase;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="zz")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="aname")]),
                    type=Reference(
                        ref_to=Array(
                            array_of=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            size=Value(tokens=[Token(value="3")]),
                        )
                    ),
                    value=Value(tokens=[Token(value="abase")]),
                ),
            ]
        )
    )


def test_var_array_of_fnptr_varargs() -> None:
    content = """
      void (*a3[3])(int, ...);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="a3")]),
                    type=Array(
                        array_of=Pointer(
                            ptr_to=FunctionType(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="void")]
                                    )
                                ),
                                parameters=[
                                    Parameter(
                                        type=Type(
                                            typename=PQName(
                                                segments=[
                                                    FundamentalSpecifier(name="int")
                                                ]
                                            )
                                        )
                                    )
                                ],
                                vararg=True,
                            )
                        ),
                        size=Value(tokens=[Token(value="3")]),
                    ),
                )
            ]
        )
    )


def test_var_double_fnptr_varargs() -> None:
    content = """
      void (*(*a4))(int, ...);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="a4")]),
                    type=Pointer(
                        ptr_to=Pointer(
                            ptr_to=FunctionType(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="void")]
                                    )
                                ),
                                parameters=[
                                    Parameter(
                                        type=Type(
                                            typename=PQName(
                                                segments=[
                                                    FundamentalSpecifier(name="int")
                                                ]
                                            )
                                        )
                                    )
                                ],
                                vararg=True,
                            )
                        )
                    ),
                )
            ]
        )
    )


def test_var_fnptr_voidstar() -> None:
    content = """
      void(*(*a5)(int));
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="a5")]),
                    type=Pointer(
                        ptr_to=FunctionType(
                            return_type=Pointer(
                                ptr_to=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="void")]
                                    )
                                )
                            ),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    )
                                )
                            ],
                        )
                    ),
                )
            ]
        )
    )


def test_var_fnptr_moreparens() -> None:
    content = """
      void (*x)(int(p1), int);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Pointer(
                        ptr_to=FunctionType(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    ),
                                    name="p1",
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    )
                                ),
                            ],
                        )
                    ),
                )
            ]
        )
    )


# From pycparser:
# Pointer decls nest from inside out. This is important when different
# levels have different qualifiers. For example:
#
#  char * const * p;
#
# Means "pointer to const pointer to char"
#
# While:
#
#  char ** const p;
#
# Means "const pointer to pointer to char"


def test_var_ptr_to_const_ptr_to_char() -> None:
    content = """
      char *const *p;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="p")]),
                    type=Pointer(
                        ptr_to=Pointer(
                            ptr_to=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="char")]
                                )
                            ),
                            const=True,
                        )
                    ),
                )
            ]
        )
    )


def test_var_const_ptr_to_ptr_to_char() -> None:
    content = """
      char **const p;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="p")]),
                    type=Pointer(
                        ptr_to=Pointer(
                            ptr_to=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="char")]
                                )
                            )
                        ),
                        const=True,
                    ),
                )
            ]
        )
    )


def test_var_array_initializer1() -> None:
    content = """
      int x[3]{1, 2, 3};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Array(
                        array_of=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        ),
                        size=Value(tokens=[Token(value="3")]),
                    ),
                    value=Value(
                        tokens=[
                            Token(value="{"),
                            Token(value="1"),
                            Token(value=","),
                            Token(value="2"),
                            Token(value=","),
                            Token(value="3"),
                            Token(value="}"),
                        ]
                    ),
                )
            ]
        )
    )


def test_var_array_initializer2() -> None:
    content = """
      int x[3] = {1, 2, 3};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Array(
                        array_of=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        ),
                        size=Value(tokens=[Token(value="3")]),
                    ),
                    value=Value(
                        tokens=[
                            Token(value="{"),
                            Token(value="1"),
                            Token(value=","),
                            Token(value="2"),
                            Token(value=","),
                            Token(value="3"),
                            Token(value="}"),
                        ]
                    ),
                )
            ]
        )
    )


def test_var_extern_c() -> None:
    content = """
      extern "C" int x;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    # TODO: store linkage
                    extern=True,
                )
            ]
        )
    )


def test_var_ns_1() -> None:
    content = """
      int N::x;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(
                        segments=[NameSpecifier(name="N"), NameSpecifier(name="x")]
                    ),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                )
            ]
        )
    )


def test_var_ns_2() -> None:
    content = """
      int N::x = 4;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(
                        segments=[NameSpecifier(name="N"), NameSpecifier(name="x")]
                    ),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(tokens=[Token(value="4")]),
                )
            ]
        )
    )


def test_var_ns_3() -> None:
    content = """
      int N::x{4};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(
                        segments=[NameSpecifier(name="N"), NameSpecifier(name="x")]
                    ),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(
                        tokens=[Token(value="{"), Token(value="4"), Token(value="}")]
                    ),
                )
            ]
        )
    )


def test_var_static_struct() -> None:
    content = """
      constexpr static struct SS {} s;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="SS")], classkey="struct"
                        )
                    )
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="s")]),
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="SS")], classkey="struct"
                        )
                    ),
                    constexpr=True,
                    static=True,
                )
            ],
        )
    )


def test_var_constexpr_enum() -> None:
    content = """
      constexpr enum E { EE } e = EE;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="E")], classkey="enum"
                    ),
                    values=[Enumerator(name="EE")],
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="e")]),
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="E")], classkey="enum"
                        )
                    ),
                    value=Value(tokens=[Token(value="EE")]),
                    constexpr=True,
                )
            ],
        )
    )


def test_var_fnptr_in_class() -> None:
    content = """
      struct DriverFuncs {
        void *(*init)();
        void (*write)(void *buf, int buflen);
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="DriverFuncs")],
                            classkey="struct",
                        )
                    ),
                    fields=[
                        Field(
                            access="public",
                            type=Pointer(
                                ptr_to=FunctionType(
                                    return_type=Pointer(
                                        ptr_to=Type(
                                            typename=PQName(
                                                segments=[
                                                    FundamentalSpecifier(name="void")
                                                ]
                                            )
                                        )
                                    ),
                                    parameters=[],
                                )
                            ),
                            name="init",
                        ),
                        Field(
                            access="public",
                            type=Pointer(
                                ptr_to=FunctionType(
                                    return_type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="void")]
                                        )
                                    ),
                                    parameters=[
                                        Parameter(
                                            type=Pointer(
                                                ptr_to=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            FundamentalSpecifier(
                                                                name="void"
                                                            )
                                                        ]
                                                    )
                                                )
                                            ),
                                            name="buf",
                                        ),
                                        Parameter(
                                            type=Type(
                                                typename=PQName(
                                                    segments=[
                                                        FundamentalSpecifier(name="int")
                                                    ]
                                                )
                                            ),
                                            name="buflen",
                                        ),
                                    ],
                                )
                            ),
                            name="write",
                        ),
                    ],
                )
            ]
        )
    )


def test_var_extern() -> None:
    content = """
      extern int externVar;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="externVar")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    extern=True,
                )
            ]
        )
    )


def test_balanced_with_gt() -> None:
    """Tests _consume_balanced_tokens handling of mismatched gt tokens"""
    content = """
      int x = (1 >> 2);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    value=Value(
                        tokens=[
                            Token(value="("),
                            Token(value="1"),
                            Token(value=">"),
                            Token(value=">"),
                            Token(value="2"),
                            Token(value=")"),
                        ]
                    ),
                )
            ]
        )
    )


def test_balanced_with_lt() -> None:
    """Tests _consume_balanced_tokens handling of mismatched lt tokens"""
    content = """
      bool z = (i < 4);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="z")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="bool")])
                    ),
                    value=Value(
                        tokens=[
                            Token(value="("),
                            Token(value="i"),
                            Token(value="<"),
                            Token(value="4"),
                            Token(value=")"),
                        ]
                    ),
                )
            ]
        )
    )


def test_balanced_bad_mismatch() -> None:
    content = """
      bool z = (12 ]);
    """
    err = "<str>:1: parse error evaluating ']': unexpected ']', expected ')'"
    with pytest.raises(CxxParseError, match=re.escape(err)):
        parse_string(content, cleandoc=True)
