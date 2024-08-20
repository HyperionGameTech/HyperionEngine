from cxxheaderparser.types import (
    AnonymousName,
    ClassDecl,
    EnumDecl,
    Enumerator,
    Field,
    ForwardDecl,
    Function,
    FundamentalSpecifier,
    NameSpecifier,
    PQName,
    Parameter,
    Pointer,
    Token,
    Type,
    Typedef,
    Value,
    Variable,
)
from cxxheaderparser.simple import (
    ClassScope,
    NamespaceScope,
    parse_string,
    ParsedData,
)


def test_basic_enum() -> None:
    content = """
      enum Foo {
        A,
        B,
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="Foo")], classkey="enum"
                    ),
                    values=[Enumerator(name="A"), Enumerator(name="B")],
                )
            ]
        )
    )


def test_enum_w_expr() -> None:
    content = """
      enum Foo {
        A = (1 / 2),
        B = 3,
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="Foo")], classkey="enum"
                    ),
                    values=[
                        Enumerator(
                            name="A",
                            value=Value(
                                tokens=[
                                    Token(value="("),
                                    Token(value="1"),
                                    Token(value="/"),
                                    Token(value="2"),
                                    Token(value=")"),
                                ]
                            ),
                        ),
                        Enumerator(name="B", value=Value(tokens=[Token(value="3")])),
                    ],
                )
            ]
        )
    )


def test_enum_w_multiline_expr() -> None:
    content = r"""
      // clang-format off
      enum Author
      {
          NAME = ('J' << 24 | \
          'A' << 16 | \
          'S' << 8 | \
          'H'),
      };
      
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="Author")], classkey="enum"
                    ),
                    values=[
                        Enumerator(
                            name="NAME",
                            value=Value(
                                tokens=[
                                    Token(value="("),
                                    Token(value="'J'"),
                                    Token(value="<<"),
                                    Token(value="24"),
                                    Token(value="|"),
                                    Token(value="'A'"),
                                    Token(value="<<"),
                                    Token(value="16"),
                                    Token(value="|"),
                                    Token(value="'S'"),
                                    Token(value="<<"),
                                    Token(value="8"),
                                    Token(value="|"),
                                    Token(value="'H'"),
                                    Token(value=")"),
                                ]
                            ),
                        )
                    ],
                )
            ]
        )
    )


def test_basic_enum_class() -> None:
    content = """
      enum class BE { BEX };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE")], classkey="enum class"
                    ),
                    values=[Enumerator(name="BEX")],
                )
            ]
        )
    )


def test_basic_enum_struct() -> None:
    content = """
      enum struct BE { BEX };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE")], classkey="enum struct"
                    ),
                    values=[Enumerator(name="BEX")],
                )
            ]
        )
    )


def test_enum_base() -> None:
    content = """
      enum class E : int {};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="E")], classkey="enum class"
                    ),
                    values=[],
                    base=PQName(segments=[FundamentalSpecifier(name="int")]),
                )
            ]
        )
    )


# instances


def test_enum_instance_1() -> None:
    content = """
      enum class BE { BEX } be1;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE")], classkey="enum class"
                    ),
                    values=[Enumerator(name="BEX")],
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="be1")]),
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="BE")], classkey="enum class"
                        )
                    ),
                )
            ],
        )
    )


def test_enum_instance_2() -> None:
    content = """
      enum class BE { BEX } be1, *be2;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE")], classkey="enum class"
                    ),
                    values=[Enumerator(name="BEX")],
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="be1")]),
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="BE")], classkey="enum class"
                        )
                    ),
                ),
                Variable(
                    name=PQName(segments=[NameSpecifier(name="be2")]),
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[NameSpecifier(name="BE")],
                                classkey="enum class",
                            )
                        )
                    ),
                ),
            ],
        )
    )


# bases in namespaces


def test_enum_base_in_ns() -> None:
    content = """
      namespace EN {
      typedef int EINT;
      };
      
      enum class BE : EN::EINT { BEX };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE")], classkey="enum class"
                    ),
                    values=[Enumerator(name="BEX")],
                    base=PQName(
                        segments=[NameSpecifier(name="EN"), NameSpecifier(name="EINT")]
                    ),
                )
            ],
            namespaces={
                "EN": NamespaceScope(
                    name="EN",
                    typedefs=[
                        Typedef(
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="EINT",
                        )
                    ],
                )
            },
        )
    )


# forward declarations


def test_enum_fwd() -> None:
    content = """
      enum class BE1;
      enum class BE2 : EN::EINT;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            forward_decls=[
                ForwardDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE1")], classkey="enum class"
                    )
                ),
                ForwardDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="BE2")], classkey="enum class"
                    ),
                    enum_base=PQName(
                        segments=[NameSpecifier(name="EN"), NameSpecifier(name="EINT")]
                    ),
                ),
            ]
        )
    )


def test_enum_private_in_class() -> None:
    content = """
      
      class C {
        enum E { E1 };
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="C")], classkey="class"
                        )
                    ),
                    enums=[
                        EnumDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="E")], classkey="enum"
                            ),
                            values=[Enumerator(name="E1")],
                            access="private",
                        )
                    ],
                )
            ]
        )
    )


def test_enum_public_in_class() -> None:
    content = """
      
      class C {
      public:
        enum E { E1 };
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="C")], classkey="class"
                        )
                    ),
                    enums=[
                        EnumDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="E")], classkey="enum"
                            ),
                            values=[Enumerator(name="E1")],
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_default_enum() -> None:
    content = """
      class A {
        enum {
          v1,
          v2,
        } m_v1 = v1;
      
        enum { vv1, vv2, vv3 } m_v2 = vv2, m_v3 = vv3;
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="A")], classkey="class"
                        )
                    ),
                    enums=[
                        EnumDecl(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="enum"
                            ),
                            values=[Enumerator(name="v1"), Enumerator(name="v2")],
                            access="private",
                        ),
                        EnumDecl(
                            typename=PQName(
                                segments=[AnonymousName(id=2)], classkey="enum"
                            ),
                            values=[
                                Enumerator(name="vv1"),
                                Enumerator(name="vv2"),
                                Enumerator(name="vv3"),
                            ],
                            access="private",
                        ),
                    ],
                    fields=[
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[AnonymousName(id=1)], classkey="enum"
                                )
                            ),
                            name="m_v1",
                            value=Value(tokens=[Token(value="v1")]),
                        ),
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[AnonymousName(id=2)], classkey="enum"
                                )
                            ),
                            name="m_v2",
                            value=Value(tokens=[Token(value="vv2")]),
                        ),
                        Field(
                            access="private",
                            type=Type(
                                typename=PQName(
                                    segments=[AnonymousName(id=2)], classkey="enum"
                                )
                            ),
                            name="m_v3",
                            value=Value(tokens=[Token(value="vv3")]),
                        ),
                    ],
                )
            ]
        )
    )


def test_enum_template_vals() -> None:
    content = """
      enum {
        IsRandomAccess = std::is_base_of<std::random_access_iterator_tag,
                                         IteratorCategoryT>::value,
        IsBidirectional = std::is_base_of<std::bidirectional_iterator_tag,
                                          IteratorCategoryT>::value,
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[
                        Enumerator(
                            name="IsRandomAccess",
                            value=Value(
                                tokens=[
                                    Token(value="std"),
                                    Token(value="::"),
                                    Token(value="is_base_of"),
                                    Token(value="<"),
                                    Token(value="std"),
                                    Token(value="::"),
                                    Token(value="random_access_iterator_tag"),
                                    Token(value=","),
                                    Token(value="IteratorCategoryT"),
                                    Token(value=">"),
                                    Token(value="::"),
                                    Token(value="value"),
                                ]
                            ),
                        ),
                        Enumerator(
                            name="IsBidirectional",
                            value=Value(
                                tokens=[
                                    Token(value="std"),
                                    Token(value="::"),
                                    Token(value="is_base_of"),
                                    Token(value="<"),
                                    Token(value="std"),
                                    Token(value="::"),
                                    Token(value="bidirectional_iterator_tag"),
                                    Token(value=","),
                                    Token(value="IteratorCategoryT"),
                                    Token(value=">"),
                                    Token(value="::"),
                                    Token(value="value"),
                                ]
                            ),
                        ),
                    ],
                )
            ]
        )
    )


def test_enum_fn() -> None:
    content = """
      enum E {
        VALUE,
      };
      
      void fn_with_enum_param1(const enum E e);
      
      void fn_with_enum_param2(const enum E e) {
        // code here
      }
      
      enum E fn_with_enum_retval1(void);
      
      enum E fn_with_enum_retval2(void) {
        // code here
      }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="E")], classkey="enum"
                    ),
                    values=[Enumerator(name="VALUE")],
                )
            ],
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn_with_enum_param1")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(
                                    segments=[NameSpecifier(name="E")], classkey="enum"
                                ),
                                const=True,
                            ),
                            name="e",
                        )
                    ],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn_with_enum_param2")]),
                    parameters=[
                        Parameter(
                            type=Type(
                                typename=PQName(
                                    segments=[NameSpecifier(name="E")], classkey="enum"
                                ),
                                const=True,
                            ),
                            name="e",
                        )
                    ],
                    has_body=True,
                ),
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="E")], classkey="enum"
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn_with_enum_retval1")]),
                    parameters=[],
                ),
                Function(
                    return_type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="E")], classkey="enum"
                        )
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn_with_enum_retval2")]),
                    parameters=[],
                    has_body=True,
                ),
            ],
        )
    )
