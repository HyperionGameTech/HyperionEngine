# Note: testcases generated via `python -m cxxheaderparser.gentest`


from cxxheaderparser.types import (
    AnonymousName,
    Array,
    BaseClass,
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
    TemplateArgument,
    TemplateSpecialization,
    Token,
    Type,
    Typedef,
    Value,
)
from cxxheaderparser.simple import ClassScope, NamespaceScope, ParsedData, parse_string


def test_simple_typedef() -> None:
    content = """
      typedef std::vector<int> IntVector;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[
                                NameSpecifier(name="std"),
                                NameSpecifier(
                                    name="vector",
                                    specialization=TemplateSpecialization(
                                        args=[
                                            TemplateArgument(
                                                arg=Type(
                                                    typename=PQName(
                                                        segments=[
                                                            FundamentalSpecifier(
                                                                name="int"
                                                            )
                                                        ]
                                                    )
                                                )
                                            )
                                        ]
                                    ),
                                ),
                            ]
                        )
                    ),
                    name="IntVector",
                )
            ]
        )
    )


def test_struct_typedef_1() -> None:
    content = """
      typedef struct {
        int m;
      } unnamed_struct, *punnamed_struct;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    fields=[
                        Field(
                            name="m",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="public",
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    name="unnamed_struct",
                ),
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="struct"
                            )
                        )
                    ),
                    name="punnamed_struct",
                ),
            ],
        )
    )


def test_struct_typedef_2() -> None:
    content = """
      typedef struct {
        int m;
      } * punnamed_struct, unnamed_struct;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    fields=[
                        Field(
                            name="m",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="public",
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="struct"
                            )
                        )
                    ),
                    name="punnamed_struct",
                ),
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    name="unnamed_struct",
                ),
            ],
        )
    )


def test_typedef_array() -> None:
    content = """
      typedef char TenCharArray[10];
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
                    type=Array(
                        array_of=Type(
                            typename=PQName(
                                segments=[FundamentalSpecifier(name="char")]
                            )
                        ),
                        size=Value(tokens=[Token(value="10")]),
                    ),
                    name="TenCharArray",
                )
            ]
        )
    )


def test_typedef_array_of_struct() -> None:
    content = """
      typedef struct{} tx[3], ty;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    )
                )
            ],
            typedefs=[
                Typedef(
                    type=Array(
                        array_of=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="struct"
                            )
                        ),
                        size=Value(tokens=[Token(value="3")]),
                    ),
                    name="tx",
                ),
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    name="ty",
                ),
            ],
        )
    )


def test_typedef_class_w_base() -> None:
    content = """
      typedef class XX : public F {} G;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="XX")], classkey="class"
                        ),
                        bases=[
                            BaseClass(
                                access="public",
                                typename=PQName(segments=[NameSpecifier(name="F")]),
                            )
                        ],
                    )
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="XX")], classkey="class"
                        )
                    ),
                    name="G",
                )
            ],
        )
    )


def test_complicated_typedef() -> None:
    content = """
      typedef int int_t, *intp_t, (&fp)(int, ulong), arr_t[10];
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name="int_t",
                ),
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        )
                    ),
                    name="intp_t",
                ),
                Typedef(
                    type=Reference(
                        ref_to=FunctionType(
                            return_type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            parameters=[
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[FundamentalSpecifier(name="int")]
                                        )
                                    )
                                ),
                                Parameter(
                                    type=Type(
                                        typename=PQName(
                                            segments=[NameSpecifier(name="ulong")]
                                        )
                                    )
                                ),
                            ],
                        )
                    ),
                    name="fp",
                ),
                Typedef(
                    type=Array(
                        array_of=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        ),
                        size=Value(tokens=[Token(value="10")]),
                    ),
                    name="arr_t",
                ),
            ]
        )
    )


def test_typedef_c_struct_idiom() -> None:
    content = """
      // common C idiom to avoid having to write "struct S"
      typedef struct {int a; int b;} S, *pS;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    fields=[
                        Field(
                            name="a",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="public",
                        ),
                        Field(
                            name="b",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="int")]
                                )
                            ),
                            access="public",
                        ),
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    name="S",
                ),
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="struct"
                            )
                        )
                    ),
                    name="pS",
                ),
            ],
        )
    )


def test_typedef_struct_same_name() -> None:
    content = """
      typedef struct Fig {
        int a;
      } Fig;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="Fig")], classkey="struct"
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
                            name="a",
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="Fig")], classkey="struct"
                        )
                    ),
                    name="Fig",
                )
            ],
        )
    )


def test_typedef_struct_w_enum() -> None:
    content = """
      typedef struct {
        enum BeetEnum : int { FAIL = 0, PASS = 1 };
      } BeetStruct;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    enums=[
                        EnumDecl(
                            typename=PQName(
                                segments=[NameSpecifier(name="BeetEnum")],
                                classkey="enum",
                            ),
                            values=[
                                Enumerator(
                                    name="FAIL", value=Value(tokens=[Token(value="0")])
                                ),
                                Enumerator(
                                    name="PASS", value=Value(tokens=[Token(value="1")])
                                ),
                            ],
                            base=PQName(segments=[FundamentalSpecifier(name="int")]),
                            access="public",
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[AnonymousName(id=1)], classkey="struct"
                        )
                    ),
                    name="BeetStruct",
                )
            ],
        )
    )


def test_typedef_union() -> None:
    content = """
      typedef union apricot_t {
        int i;
        float f;
        char s[20];
      } Apricot;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="apricot_t")], classkey="union"
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
                            name="i",
                        ),
                        Field(
                            access="public",
                            type=Type(
                                typename=PQName(
                                    segments=[FundamentalSpecifier(name="float")]
                                )
                            ),
                            name="f",
                        ),
                        Field(
                            access="public",
                            type=Array(
                                array_of=Type(
                                    typename=PQName(
                                        segments=[FundamentalSpecifier(name="char")]
                                    )
                                ),
                                size=Value(tokens=[Token(value="20")]),
                            ),
                            name="s",
                        ),
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="apricot_t")], classkey="union"
                        )
                    ),
                    name="Apricot",
                )
            ],
        )
    )


def test_typedef_fnptr() -> None:
    content = """
      typedef void *(*fndef)(int);
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
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
                    name="fndef",
                )
            ]
        )
    )


def test_typedef_const() -> None:
    content = """
      typedef int theint, *const ptheint;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[FundamentalSpecifier(name="int")])
                    ),
                    name="theint",
                ),
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
                        ),
                        const=True,
                    ),
                    name="ptheint",
                ),
            ]
        )
    )


def test_enum_typedef_1() -> None:
    content = """
      typedef enum {} E;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[AnonymousName(id=1)], classkey="enum")
                    ),
                    name="E",
                )
            ],
        )
    )


def test_enum_typedef_2() -> None:
    content = """
      typedef enum { E1 } BE;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[Enumerator(name="E1")],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[AnonymousName(id=1)], classkey="enum")
                    ),
                    name="BE",
                )
            ],
        )
    )


def test_enum_typedef_3() -> None:
    content = """
      typedef enum { E1, E2, } E;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[Enumerator(name="E1"), Enumerator(name="E2")],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[AnonymousName(id=1)], classkey="enum")
                    ),
                    name="E",
                )
            ],
        )
    )


def test_enum_typedef_3_1() -> None:
    content = """
      typedef enum { E1 } * PBE;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[Enumerator(name="E1")],
                )
            ],
            typedefs=[
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="enum"
                            )
                        )
                    ),
                    name="PBE",
                )
            ],
        )
    )


def test_enum_typedef_4() -> None:
    content = """
      typedef enum { E1 } * PBE, BE;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[Enumerator(name="E1")],
                )
            ],
            typedefs=[
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="enum"
                            )
                        )
                    ),
                    name="PBE",
                ),
                Typedef(
                    type=Type(
                        typename=PQName(segments=[AnonymousName(id=1)], classkey="enum")
                    ),
                    name="BE",
                ),
            ],
        )
    )


def test_enum_typedef_5() -> None:
    content = """
      typedef enum { E1 } BE, *PBE;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[Enumerator(name="E1")],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[AnonymousName(id=1)], classkey="enum")
                    ),
                    name="BE",
                ),
                Typedef(
                    type=Pointer(
                        ptr_to=Type(
                            typename=PQName(
                                segments=[AnonymousName(id=1)], classkey="enum"
                            )
                        )
                    ),
                    name="PBE",
                ),
            ],
        )
    )


def test_enum_typedef_fwd() -> None:
    content = """
      typedef enum BE BET;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[NameSpecifier(name="BE")], classkey="enum"
                        )
                    ),
                    name="BET",
                )
            ]
        )
    )


def test_typedef_enum_expr() -> None:
    content = """
      typedef enum { StarFruit = (2 + 2) / 2 } Carambola;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            enums=[
                EnumDecl(
                    typename=PQName(segments=[AnonymousName(id=1)], classkey="enum"),
                    values=[
                        Enumerator(
                            name="StarFruit",
                            value=Value(
                                tokens=[
                                    Token(value="("),
                                    Token(value="2"),
                                    Token(value="+"),
                                    Token(value="2"),
                                    Token(value=")"),
                                    Token(value="/"),
                                    Token(value="2"),
                                ]
                            ),
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(segments=[AnonymousName(id=1)], classkey="enum")
                    ),
                    name="Carambola",
                )
            ],
        )
    )


def test_volatile_typedef() -> None:
    content = """
      typedef volatile signed short vint16;
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            typedefs=[
                Typedef(
                    type=Type(
                        typename=PQName(
                            segments=[FundamentalSpecifier(name="signed short")]
                        ),
                        volatile=True,
                    ),
                    name="vint16",
                )
            ]
        )
    )


def test_function_typedef() -> None:
    content = """
      typedef void fn(int);
      typedef auto fntype(int) -> int;
      
      struct X {
          typedef void fn(int);
      };
    """
    data = parse_string(content, cleandoc=True)

    assert data == ParsedData(
        namespace=NamespaceScope(
            classes=[
                ClassScope(
                    class_decl=ClassDecl(
                        typename=PQName(
                            segments=[NameSpecifier(name="X")], classkey="struct"
                        )
                    ),
                    typedefs=[
                        Typedef(
                            type=FunctionType(
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
                            ),
                            name="fn",
                            access="public",
                        )
                    ],
                )
            ],
            typedefs=[
                Typedef(
                    type=FunctionType(
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
                                )
                            )
                        ],
                    ),
                    name="fn",
                ),
                Typedef(
                    type=FunctionType(
                        return_type=Type(
                            typename=PQName(segments=[FundamentalSpecifier(name="int")])
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
                        has_trailing_return=True,
                    ),
                    name="fntype",
                ),
            ],
        )
    )
