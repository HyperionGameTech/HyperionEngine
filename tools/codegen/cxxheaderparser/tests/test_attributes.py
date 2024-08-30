# Note: testcases generated via `python -m cxxheaderparser.gentest`

from cxxheaderparser.types import (
    ClassDecl,
    EnumDecl,
    Enumerator,
    Field,
    FriendDecl,
    Function,
    FundamentalSpecifier,
    Method,
    NameSpecifier,
    PQName,
    Pointer,
    TemplateDecl,
    TemplateTypeParam,
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


def test_attributes_everywhere() -> None:
    # TODO: someday we'll actually support storing attributes,
    #       but for now just make sure they don't get in the way

    content = """
      struct [[deprecated]] S {};
      [[deprecated]] typedef S *PS;
      
      [[deprecated]] int x;
      union U {
        [[deprecated]] int n;
      };
      [[deprecated]] void f();
      
      enum [[deprecated]] E{A [[deprecated]], B [[deprecated]] = 42};
      
      struct alignas(8) AS {};
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="S")], classkey="struct"
                        )
                    )
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="U")], classkey="union"
                        )
                    ),
                    fields=[
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            name="n",
                        )
                    ],
                ),
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="AS")], classkey="struct"
                        )
                    )
                ),
            ],
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="E")], classkey="enum"
                    ),
                    values=[
                        Enumerator(name="A"),
                        Enumerator(name="B", value=Value(tokens=[Token(value="42")])),
                    ],
                )
            ],
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="void")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="f")]),
                    parameters=[],
                )
            ],
            typedefs=[
                Typedef(
                    type=Pointer(
                        ptr_to=Type(typename=PQName(segments=[NameSpecifier(name="S")]))
                    ),
                    name="PS",
                )
            ],
            variables=[
                Variable(
                    name=PQName(segments=[NameSpecifier(name="x")]),
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                )
            ],
        )
    )


def test_attributes_gcc_enum_packed() -> None:
    content = """
      enum Wheat {
        w1,
        w2,
        w3,
      } __attribute__((packed));
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(
                        segments=[NameSpecifier(name="Wheat")], classkey="enum"
                    ),
                    values=[
                        Enumerator(name="w1"),
                        Enumerator(name="w2"),
                        Enumerator(name="w3"),
                    ],
                )
            ]
        )
    )


def test_friendly_declspec() -> None:
    content = """
      struct D {
          friend __declspec(dllexport) void my_friend();
          static __declspec(dllexport) void static_declspec();
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="D")], classkey="struct"
                        )
                    ),
                    friends=[
                        FriendDecl(
                            fn=Method(
                                return_type=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="void")]
                                    )
                                ),
                                name=PQName(segments=[NameSpecifier(name="my_friend")]),
                                parameters=[],
                                access="public",
                            )
                        )
                    ],
                    methods=[
                        Method(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="void")]
                                )
                            ),
                            name=PQName(
                                segments=[NameSpecifier(name="static_declspec")]
                            ),
                            parameters=[],
                            static=True,
                            access="public",
                        )
                    ],
                )
            ]
        )
    )


def test_declspec_template() -> None:
    content = """
      template <class T2>
      __declspec(deprecated("message"))
      static T2 fn() { return T2(); }
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            functions=[
                Function(
                    return_type=Type(
                        typename=PQName(segments=[NameSpecifier(name="T2")])
                    ),
                    name=PQName(segments=[NameSpecifier(name="fn")]),
                    parameters=[],
                    static=True,
                    has_body=True,
                    template=TemplateDecl(
                        params=[TemplateTypeParam(typekey="class", name="T2")]
                    ),
                )
            ]
        )
    )
